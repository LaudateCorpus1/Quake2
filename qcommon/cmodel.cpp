/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// cmodel.cpp -- collision model loading / processing

/*?? TODO:
  - add functions:
	- CM_PointCluster(CVec3&) -- PointLeafnum()+LeafCluster()
	- CM_BoxClusters(CBox&, dst) -- BoxLeafnums()+LeafCluster()+compress list (no duplicated clusters) -- used few times in server
	- CM_BoxPVS(CBox&, dst) -- have SV_FatPVS() func; may be useful for renderer too
	? CM_InPVS(pos1, pos2) -- have func in sv_game.cpp (uses PVS + areas)
*/


#include "qcommon.h"

#include "../client/ref.h"	//!!!!! using RE_DrawTextXxx () for debug <------ REMOVE!
bool SHOW_TRACE; //!!!! REMOVE

#include "cmodel.h"
#include "MapBrush.h"


struct cnode_t
{
	cplane_t *plane;
	int		children[2];				// negative numbers are leafs
};

struct cbrushside_t
{
	cplane_t *plane;
	csurface_t *surface;
};

struct cbrush_t
{
	int		contents;
	int		numsides;
	int		firstbrushside;
	int		traceFrame;					// to avoid repeated testings
};

struct carea_t
{
	int		numareaportals;
	int		firstareaportal;
	int		floodnum;					// if two areas have equal floodnums, they are connected
	int		floodvalid;
};

char	map_name[MAX_QPATH];
static CMemoryChain *dataChain;
bspfile_t *map_bspfile;
bool	map_clientLoaded;

int		c_traces, c_pointcontents;		// stats


static int		numBrushSides;
static cbrushside_t *map_brushSides;

int				numTexinfo;				// global (for precache)
csurface_t		*map_surfaces;			// global (for precache)

static int		numPlanes;
static cplane_t	*map_planes;

static int		numNodes;
static cnode_t	*map_nodes;

static int		numLeafs;
static dBsp2Leaf_t *map_leafs;

static int		emptyLeaf;				//?? used for BoxHull

static int		numLeafBrushes;
static unsigned short *map_leafBrushes;

static int		numcmodels;
static cmodel_t	*map_cmodels;

static int		numBrushes;
static cbrush_t	*map_brushes;

static int		numentitychars;
static const char *map_entitystring;

static int		numAreas;
static carea_t	*map_areas;

static int		numareaportals;
static dareaportal_t *map_areaportals;

static int		numclusters;

static csurface_t nullsurface;

static int		floodvalid;

static bool portalopen[MAX_MAP_AREAPORTALS];


static cvar_t	*map_noareas;

// forwards
static void InitBoxHull ();
static void FloodAreaConnections ();



/*-----------------------------------------------------------------------------
	Material by texture
-----------------------------------------------------------------------------*/


class surfMaterial_t : public CStringItem
{
public:
	material_t material;
};


static CMemoryChain *surfMaterialChain;
static TList<surfMaterial_t> surfMaterialList;


static void ReadSurfMaterials (const char *filename)
{
	if (surfMaterialChain)
	{
		delete surfMaterialChain;
		surfMaterialChain = NULL;
		surfMaterialList.Reset();
	}

	char *file;
	if (!(file = (char*) GFileSystem->LoadFile (filename)))
	{
		Com_DPrintf ("ReadSurfMaterials: %s is not found\n", filename);
		return;
	}

	surfMaterialChain = new CMemoryChain;
	surfMaterial_t *prev = NULL;
	const char *in = file;

	const char *s;
	while (s = COM_Parse (in), in)
	{
		material_t	m;

		// s points to material name string
		switch (toLower (s[0]))
		{
#define T(c,mat) case c: m = MATERIAL_##mat; break;
		T('m',METAL);
		T('t',TILE);
		T('y',GLASS);
		T('l',GRAVEL);
		T('d',DIRT);
		T('c',CONCRETE);
		T('w',WOOD);
		T('n',SNOW);
		T('z',SILENT);
#undef T
		default:
			appWPrintf ("Unknown material mark \"%s\" in %s\n", s, filename);
			m = MATERIAL_CONCRETE;
		}

		s = COM_Parse (in);
		if (!in)
		{
			appWPrintf ("Unexpected end of file %s\n", filename);
			break;
		}
		// s points to surface name
		surfMaterial_t *sm = new (s, surfMaterialChain) surfMaterial_t;
		sm->material = m;
		// add to list
		surfMaterialList.InsertAfter (sm, prev);

		prev = sm;
//		Com_DPrintf ("Added surface material: %d for %s\n", m, s);
	}

	delete file;
}

// name must be without "textures/" prefix and without extension
static material_t GetSurfMaterial (const char *name)
{
	const char *checkname = strrchr (name, '/');		// points to a name without path
	if (checkname)
		checkname++;
	else
		checkname = name;

	for (TListIterator<surfMaterial_t> sm = surfMaterialList; sm; ++sm)
	{
		const char *s = sm->name;
		if (strchr (s, '/'))
		{	// mask have a path separator - compare full names
			if (!appMatchWildcard (name, sm->name, true))
				continue;
		}
		else
		{	// compare without path
			if (!appMatchWildcard (checkname, sm->name, true))
				continue;
		}

		material_t m = sm->material;
//		Com_DPrintf ("set material %d for %s\n", m, name);
		return m;
	}
//	Com_DPrintf ("set default material for %s\n", name);
	return MATERIAL_CONCRETE;	// default material
}


/*-----------------------------------------------------------------------------
	q2 bsp map loading
-----------------------------------------------------------------------------*/

inline void LoadSubmodels (cmodel_t *data, int count)
{
	map_cmodels = data;
	numcmodels  = count;
	if (map_cmodels[0].headnode)
		Com_DropError ("map has invalid headnode = %d", map_cmodels[0].headnode);
}


static void LoadSurfaces2 (dBsp2Texinfo_t *data, int size)
{
	if (size < 1) Com_DropError ("Map with no surfaces");

	ReadSurfMaterials ("sound/materials.lst");

	dBsp2Texinfo_t *in = data;
	numTexinfo = size;
	csurface_t	*out;
	out = map_surfaces = new (dataChain) csurface_t [size];

	for (int i = 0; i < size; i++, in++, out++)
	{
		appStrncpyz (out->shortName, in->texture, sizeof(out->shortName));	// texture name limited to 16 chars (compatibility with older mods?)
		appStrncpyz (out->fullName, in->texture, sizeof(out->fullName));	// name limited to 32 chars (w/o "textures/" and ".wal")
		int f = in->flags;
		out->flags = f & ~SURF_KP_MATERIAL;
		out->value = in->value;

		int m;
		if		(f & SURF_CONCRETE)	m = MATERIAL_CONCRETE;
		else if (f & SURF_FABRIC)	m = MATERIAL_FABRIC;
		else if (f & SURF_GRAVEL)	m = MATERIAL_GRAVEL;
		else if (f & SURF_METAL) 	m = MATERIAL_METAL;
		else if (f & SURF_METAL_L)	m = MATERIAL_METAL_L;
		else if (f & SURF_SNOW)		m = MATERIAL_TIN;		//?? MATERIAL_SNOW;
		else if (f & SURF_TILE)		m = MATERIAL_TILE;
		else if (f & SURF_WOOD)		m = MATERIAL_WOOD;
		else if (f & SURF_WATER)	m = MATERIAL_WATER;
		else						m = GetSurfMaterial (in->texture);
		out->material = m;
	}
}


static void LoadNodes2 (dBsp2Node_t *data, int size)
{
	dBsp2Node_t *in = data;

	if (size < 1) Com_DropError ("map has no nodes");

	cnode_t *out;
	out = map_nodes = new (dataChain) cnode_t [size + 6];		// extra for box hull

	numNodes = size;
	for (int i = 0; i < size; i++, out++, in++)
	{
		out->plane       = map_planes + in->planenum;
		out->children[0] = in->children[0];
		out->children[1] = in->children[1];
	}
}


static void LoadBrushes2 (dBsp2Brush_t *data, int size)
{
	dBsp2Brush_t *in = data;

	cbrush_t	*out;
	out = map_brushes = new (dataChain) cbrush_t [size + 1];	// extra for box hull
	numBrushes = size;

	for (int i = 0; i < size; i++, out++, in++)
	{
		out->firstbrushside = in->firstside;
		out->numsides       = in->numsides;
		out->contents       = in->contents;
	}
}


static void LoadLeafs2 (dBsp2Leaf_t *data, int size)
{
	if (size < 1) Com_DropError ("Map with no leafs");

#if 0
	map_leafs = data; -- cannot do this
#else
	map_leafs = new (dataChain) dBsp2Leaf_t [size + 1];		// extra for box hull
	memcpy (map_leafs, data, sizeof(dBsp2Leaf_t)*size);		// must perform this, because needs 1 more leaf for InitBoxHull()
#endif
	numLeafs = size;

	if (map_leafs[0].contents != CONTENTS_SOLID)
		Com_DropError ("map leaf 0 is not CONTENTS_SOLID");
	// find any empty leaf
	emptyLeaf = -1;
	for (int i = 1; i < numLeafs; i++)
	{
		if (!map_leafs[i].contents)
		{
			emptyLeaf = i;
			break;
		}
	}
	if (emptyLeaf == -1)
		Com_DropError ("map does not have an empty leaf");
}


static void LoadPlanes (dPlane_t *data, int size)
{
	dPlane_t *in = data;

	if (size < 1) Com_DropError ("Map with no planes");

	cplane_t *out;
	staticAssert(sizeof(cplane_t) == sizeof(dPlane_t), sizeof_cplane_same_as_dplane);
	// can simply set map_planes=data; note: "data" is dPlane_t, but "map_planes" is cplane_t,
	// but both have the same size (see qfiles.h for details) and the same layout
	out = map_planes = (cplane_t*)data;
	for (int i = 0; i < size; i++, out++)
	{
		out->SetType ();
		out->SetSignbits ();
	}
	numPlanes = size;
}


inline void LoadLeafBrushes (unsigned short *data, int size)
{
	if (size < 1) Com_DropError ("Map with no leafbrushes");

#if 0
	map_leafBrushes = data; -- can't do this, because we need 1 more for InitBoxHull()
#else
	map_leafBrushes = new (dataChain) unsigned short [size+1];	// extra for box hull
	memcpy (map_leafBrushes, data, sizeof(*data)*size);
#endif
	numLeafBrushes = size;
}


static void LoadBrushSides2 (dBsp2Brushside_t *data, int size)
{
	dBsp2Brushside_t *in = data;

	cbrushside_t *out;
	out = map_brushSides = new (dataChain) cbrushside_t [size + 6];		// extra for box hull
	numBrushSides = size;

	for (int i = 0; i < size; i++, in++, out++)
	{
		out->plane = &map_planes[in->planenum];
		int j = in->texinfo;
		if (j >= numTexinfo)
			Com_DropError ("Bad brushside texinfo");
		out->surface = &map_surfaces[j];
	}
}


static void LoadAreas (darea_t *data, int size)
{
	darea_t *in = data;

	carea_t *out;
	out = map_areas = new (dataChain) carea_t [size];
	numAreas = size;

	for (int i = 0; i < size; i++, in++, out++)
	{
		out->numareaportals  = in->numareaportals;
		out->firstareaportal = in->firstareaportal;
		out->floodvalid      = 0;
		out->floodnum        = 0;
	}
}


inline void LoadAreaPortals (dareaportal_t *data, int size)
{
	numareaportals  = size;
	map_areaportals = data;
}


inline void LoadEntityString (const char *data, int size)
{
	numentitychars   = size;
	map_entitystring = data;
}


static void LoadQ2Map (bspfile_t *bsp)
{
	LoadPlanes (bsp->planes, bsp->numPlanes);
	LoadNodes2 (bsp->nodes2, bsp->numNodes);
	LoadSurfaces2 (bsp->texinfo2, bsp->numTexinfo);
	LoadLeafs2 (bsp->leafs2, bsp->numLeafs);
	LoadLeafBrushes (bsp->leafbrushes, bsp->numLeafbrushes);
	LoadBrushes2 (bsp->brushes, bsp->numBrushes);
	LoadBrushSides2 (bsp->brushsides, bsp->numBrushSides);
	LoadSubmodels (bsp->models, bsp->numModels);
	LoadAreas (bsp->areas, bsp->numAreas);
	LoadAreaPortals (bsp->areaportals, bsp->numAreaportals);
	LoadEntityString (bsp->entStr, bsp->entStrSize);

	InitBoxHull ();

	memset (portalopen, 0, sizeof(portalopen));
	FloodAreaConnections ();
}


/*-----------------------------------------------------------------------------
	q1/hl bsp map loading
-----------------------------------------------------------------------------*/

static void LoadSurfaces1 (dBsp1Texinfo_t *data, int size)
{
	if (size < 1) Com_DropError ("Map with no surfaces");

	//?? hack (convert bsp1 -> bsp2 texinfo) ?
	dBsp1Texinfo_t *in = data;
	dBsp2Texinfo_t *out;
	out = map_bspfile->texinfo2 = new (dataChain) dBsp2Texinfo_t [size];

	for (int i = 0; i < size; i++, in++, out++)
	{
		// find miptex
		int miptex = in->miptex;
		dBsp1Miptex_t *tex = (dBsp1Miptex_t*)( (byte*)map_bspfile->miptex1 + map_bspfile->miptex1->dataofs[miptex] );
		appStrncpyz (out->texture, tex->name, sizeof(out->texture));
		memcpy (out->vecs, in->vecs, sizeof(out->vecs));
		// compute surface flags
		unsigned flags = 0;
		if ((out->texture[0] == '*' && map_bspfile->type == map_q1) ||
			(out->texture[0] == '!' && map_bspfile->type == map_hl))
			flags |= SURF_WARP;
		else if (out->texture[0] == '{' && map_bspfile->type == map_hl)
			flags |= SURF_ALPHA;
		else if (!strncmp (out->texture, "sky", 3))
			flags |= SURF_SKY;
		out->flags = flags;
		out->value = miptex;		// index in textures lump
		out->nexttexinfo = -1;		// no next texture: animation will be processed later
	}
	// load Q2 textures
	LoadSurfaces2 (map_bspfile->texinfo2, size);
}


static void LoadNodes1 (dBsp1Node_t *data, int size)
{
	dBsp1Node_t *in = data;

	if (size < 1) Com_DropError ("map has no nodes");

	cnode_t *out;
	out = map_nodes = new (dataChain) cnode_t [size + 6];		// extra for box hull

	numNodes = size;
	for (int i = 0; i < size; i++, out++, in++)
	{
		out->plane       = map_planes + in->planenum;
		out->children[0] = in->children[0];
		out->children[1] = in->children[1];
	}
}


static void LoadLeafs1 (dBsp1Leaf_t *data, int size)
{
	if (size < 1) Com_DropError ("Map with no leafs");

	int i;

	// count number of leafs, which should be generated: q1 bsp uses single leaf with
	// CONTENTS_SOLID - so, we should clone this leaf ...
	int numSolidLeafs = 0;
	for (i = 0; i < numNodes; i++)
	{
		cnode_t *node = map_nodes + i;
		// leaf[0] should be CONTENTS_SOLID (checked below)
		// count number of links to leaf[0] and replace with new leaf index
		// (leafs will be generated later)
		if (node->children[0] == -1)
			node->children[0] = -1 - (size + numSolidLeafs++);
		if (node->children[1] == -1)
			node->children[1] = -1 - (size + numSolidLeafs++);
	}
	Com_DPrintf ("Found %d solid leafs (have %d normal leafs)\n", numSolidLeafs, size);

	dBsp2Leaf_t *out;
	out = map_leafs = new (dataChain) dBsp2Leaf_t [size + numSolidLeafs + 1];	// extra for box hull
	numLeafs = size + numSolidLeafs;

	dBsp1Leaf_t *in;
	emptyLeaf = -1;
	int numClipLeafs = 0;
	for (i = 0, in = data; i < size; i++, in++, out++)
	{
//		memcpy (out->mins, in->mins, sizeof(short)*6);	// copy mins[]+maxs[] -- unused here
		// find corresponding Q2 contents
		static const int q1Contents[] =
		{
			0,							// -1 (EMPTY)
			CONTENTS_SOLID,				// -2 (SOLID)
			CONTENTS_WATER,				// -3 (WATER)
			CONTENTS_SLIME,				// ...
			CONTENTS_LAVA,
			CONTENTS_MONSTER,			// SKY
			CONTENTS_ORIGIN,			// removed while bsp
			CONTENTS_SOLID,				// CLIP
			CONTENTS_CURRENT_0|CONTENTS_WATER,
			CONTENTS_CURRENT_90|CONTENTS_WATER,
			CONTENTS_CURRENT_180|CONTENTS_WATER,
			CONTENTS_CURRENT_270|CONTENTS_WATER,
			CONTENTS_CURRENT_UP|CONTENTS_WATER,
			CONTENTS_CURRENT_DOWN|CONTENTS_WATER,
			CONTENTS_TRANSLUCENT		// -15
		};
		if (in->contents >= 0 || in->contents < -15)
			Com_DropError ("unknown contents in Q1/HL map: %d", in->contents);
		out->contents       = q1Contents[-in->contents - 1];	// -1 => 0, -2 => 1 ...
		out->area           = 0;
		out->numleafbrushes = 0;

		if (out->contents == CONTENTS_SOLID)
			out->cluster = -1;
		else if (map_bspfile->visInfo)
			out->cluster = i-1;			// == leaf number -1 (no visinfo for leaf[0]==SOLID); visinfo generated in models.cpp
		// else - cluster = 0
		//?? use in->ambient_level[]
		// find empty leaf
		if (!out->contents && emptyLeaf < 0)
			emptyLeaf = i;
		// count other non-empty leafs
		if (out->contents && out->contents != CONTENTS_SOLID)
			numClipLeafs++;
	}

	// generate solid leafs
	for (i = 0; i < numSolidLeafs; i++, out++)
	{
		out->contents       = CONTENTS_SOLID;
		out->area           = 0;
		out->numleafbrushes = 0;	// will be set to 1 later
		out->cluster        = -1;
	}

	numBrushes = numSolidLeafs + numClipLeafs;
	// create leafbrushes
	map_leafBrushes = new (dataChain) unsigned short [numBrushes+1]; // extra for box hull
	numLeafBrushes  = numBrushes;
	for (i = 0; i < numBrushes; i++)
		map_leafBrushes[i] = i;

#if 0
	int minCounts[16], surfCounts[16], leafCounts[16];
	memset(minCounts,0x7F,16*4);
	memset(surfCounts,0,16*4);
	memset(leafCounts,0,16*4);
	const char *q1names[] = {
		"??", "EMPTY", "SOLID", "WATER", "SLIME", "LAVA", "SKY", "ORIGIN", "CLIP",
		"CURR_0", "CURR_90", "CURR_180", "CURR_270", "CURR_UP", "CURR_DOWN", "TRANS"
	};
	for (i = 0, in = data; i < size; i++, in++)
	{
		int n = -in->contents;
		minCounts[n] = min(minCounts[n], in->numleaffaces);
		surfCounts[n] += in->numleaffaces;
		leafCounts[n]++;
	}
	for (i = 0; i < 16; i++)
		if (leafCounts[i])
			appPrintf ("%2d/%10s: min=%5d  surf=%5d  leaf=%5d\n", i, q1names[i], minCounts[i], surfCounts[i], leafCounts[i]);
#endif

	if (map_leafs[0].contents != CONTENTS_SOLID)
		Com_DropError ("map leaf 0 is not CONTENTS_SOLID");

	if (emptyLeaf == -1)
		Com_DropError ("map does not have an empty leaf");
}


static cplane_t *FindPlane (const CVec3 &norm, float dist)
{
	cplane_t *p2 = map_planes;
	for (int i = 0; i < numPlanes; i++, p2++)
	{
		if (p2->dist   != dist) continue;
		if (p2->normal == norm) return p2;
	}
	//?? can remember new planes too, and use them when possible
	p2 = new (dataChain) cplane_t;
	p2->normal = norm;
	p2->dist   = dist;
	p2->SetType ();
	p2->SetSignbits ();
	return p2;
}

static cplane_t *FindBackplane (cplane_t *plane)
{
	cplane_t *p2 = map_planes;
	CVec3 norm;
	VectorNegate (plane->normal, norm);
	float dist = -plane->dist;
	for (int i = 0; i < numPlanes; i++, p2++)
	{
		if (p2->dist   != dist) continue;
		if (p2->normal == norm) return p2;
	}
	//?? can remember new planes too, and use them when possible
	p2 = new (dataChain) cplane_t;
	p2->normal = norm;
	p2->dist   = dist;
	p2->SetType ();
	p2->SetSignbits ();
	return p2;
}


static CBrush *GetHugeBrush ()
{
#define BSIZE		65536
	// create large brush: should be allocated dynamically, because it will
	// be modified and stored to leafBrushes[]
	static const CBox largeBounds = {{-BSIZE,-BSIZE,-BSIZE}, {BSIZE,BSIZE,BSIZE}};
	return new (CBrush::mem) CBrush (largeBounds);
}


static void BuildBrushes1 (int numLeafsOrig)
{
	guard(BuildBrushes1);

	// prepare
	CBrush::mem = new CMemoryChain;
	int64 time = appCycles64 ();

	map_brushes = new (dataChain) cbrush_t [numBrushes+1];	// extra for box hull

	int numBuiltBrushes = 0;
	numBrushSides = 0;
	CBrush *leafBrushes[MAX_MAP_LEAFS];
	memset (leafBrushes, 0, sizeof(leafBrushes));
	for (int model = 0; model < numcmodels; model++)
	{
		int	stack[MAX_TREE_DEPTH], sptr = 0;		// stack
		CBrush *stack2[MAX_TREE_DEPTH];
		int nodenum = map_cmodels[model].headnode;

		CBrush *brush = GetHugeBrush ();
		while (true)
		{
			if (nodenum < 0)
			{
				int leafnum = -1 - nodenum;
				assert(leafnum > 0 && leafnum < numLeafs);	// leaf[0] should be replaced
				const dBsp2Leaf_t *leaf = map_leafs + leafnum;
				if (leaf->contents)					// non-empty leafs
				{
					// remember brush
					assert(!leafBrushes[leafnum]);
					// build bevels for correct trace() against box
					brush->AddBevels (FindPlane);
					leafBrushes[leafnum] = brush;
					// count number of brush sides
					for (CBrushSide *s = brush->sides; s; s = s->next)
						if (s->plane->dist != BSIZE)
							numBrushSides++;
				}

				if (!sptr)
					break;							// whole tree visited
				else
				{
					sptr--;
					nodenum = stack[sptr];
					brush   = stack2[sptr];
					continue;
				}
			}

			const cnode_t &node = map_nodes[nodenum];
			// split brush with plane
			CBrush *backBrush = brush->Split (node.plane);
			if (!backBrush) Com_DropError ("NULL backBrush");
			// remember back side ...
			stack[sptr]  = node.children[1];
			stack2[sptr] = backBrush;
			sptr++;
			// and continue with front side
			nodenum = node.children[0];
		}
	}

	// alloc brushsides
	map_brushSides = new (dataChain) cbrushside_t [numBrushSides+6];	// extra for box hull

	// convert CBrush to cbrush_t and cbrushside_t
	int brushSideIdx = 0;
	dBsp2Leaf_t *leaf = map_leafs;
	cbrushside_t *bs  = map_brushSides;
	cbrush_t *dst     = map_brushes;
	for (int i = 0; i < numLeafs; i++, leaf++)
	{
		if (!leaf->contents || (leaf->contents == CONTENTS_SOLID && i < numLeafsOrig))
			continue;

		leaf->firstleafbrush = dst - map_brushes;	// == brush number
		leaf->numleafbrushes = 1;

		dst->contents = leaf->contents;
		CBrush *brush = leafBrushes[i];
		if (!brush)
			Com_DropError ("brush %d not generated", i);
		dst->firstbrushside = brushSideIdx;
		for (CBrushSide *s = brush->sides; s; s = s->next)
			if (s->plane->dist != BSIZE)			// ignore largeBounds sides
			{
				bs->plane   = (s->back) ? FindBackplane (s->plane) : s->plane;
				bs->surface = &nullsurface;			//?? texture
				bs++; brushSideIdx++;				// next brushside
				dst->numsides++;
			}
		dst++;										// next brush
	}

	// finish
	time = appCycles64 () - time;
	Com_DPrintf ("Built %d brushes in %g msec, used temporary memory %dKb\n",
		numBrushes, appDeltaCyclesToMsecf (time), CBrush::mem->GetSize() >> 10);
	delete CBrush::mem;

	unguard;
}


static void LoadQ1Map (bspfile_t *bsp)
{
	LoadPlanes (bsp->planes, bsp->numPlanes);
	LoadNodes1 (bsp->nodes1, bsp->numNodes);
	LoadSurfaces1 (bsp->texinfo1, bsp->numTexinfo);
	LoadLeafs1 (bsp->leafs1, bsp->numLeafs);
	LoadSubmodels (bsp->models, bsp->numModels);
	BuildBrushes1 (bsp->numLeafs);

#if MAX_DEBUG
	if (map_cmodels[0].headnode)
		Com_DropError ("Q1/HL map has invalid headnode = %d", map_cmodels[0].headnode);
#endif

	// Q1 areas: simulate loading (create 1 area)
	map_areas = new (dataChain) carea_t;
	numAreas = 1;
	map_areas[0].numareaportals  = 0;
	map_areas[0].firstareaportal = 0;
	map_areas[0].floodvalid      = 0;
	map_areas[0].floodnum        = 0;
	numareaportals = 0;

	LoadEntityString (bsp->entStr, bsp->entStrSize);

	InitBoxHull ();

	memset (portalopen, 0, sizeof(portalopen));
	FloodAreaConnections ();
}


/*-----------------------------------------------------------------------------
	Common map loading code
-----------------------------------------------------------------------------*/

// Loads in the map and all submodels
cmodel_t *CM_LoadMap (const char *name, bool clientload, unsigned *checksum)
{
	guard(CM_LoadMap);

	map_noareas = Cvar_Get ("map_noareas", "0");

	static unsigned	last_checksum;
	if (map_name[0] && !stricmp (map_name, name) && (clientload || !Cvar_VariableInt ("flushmap")))		//?? rename flushmap -> map_flush
	{
		*checksum = last_checksum;
		if (!clientload)
		{
			memset (portalopen, 0, sizeof(portalopen));
			FloodAreaConnections ();
		}
		return &map_cmodels[0];
	}

	// free old stuff
	numPlanes = numNodes = numLeafs = numcmodels = numentitychars = 0;
	map_entitystring = "";
	map_name[0] = 0;
	map_bspfile = NULL;
	if (dataChain) delete dataChain;
	dataChain = NULL;

	if (!name || !name[0])
	{
		numclusters = 0;
		numAreas    = 0;
		*checksum   = 0;
		map_clientLoaded = false;
		return &map_cmodels[0];			// cinematic servers won't have anything at all
	}

	bspfile_t *bsp = map_bspfile = LoadBspFile (name, clientload, &last_checksum);
	dataChain = new CMemoryChain;
	*checksum = last_checksum;

	numclusters = bsp->numClusters;

	switch (bsp->type)
	{
	case map_q2:
	case map_kp:
		LoadQ2Map (bsp);
		break;
	case map_q1:
	case map_hl:
		LoadQ1Map (bsp);
		break;
	default:
		Com_DropError ("unknown bsp->type for %s", name);
	}

	strcpy (map_name, name);
	return &map_cmodels[0];

	unguardf(("%s", name));
}


/*-----------------------------------------------------------------------------
	Debugging brush/node/leaf volumes
-----------------------------------------------------------------------------*/

#if !NO_DEBUG

CBrush *CM_BuildBrush (int brushNum, CMemoryChain *mem)
{
	if (brushNum < 0 || brushNum >= numBrushes) return NULL;

	guard(CM_BuildBrush);

	CBrush::mem = mem;
	CBrush *brush = GetHugeBrush ();

	cbrush_t &b = map_brushes[brushNum];
	int i;
	cbrushside_t *side;
	for (i = 0, side = &map_brushSides[b.firstbrushside]; i < b.numsides; i++, side++)
	{
		cplane_t *plane = side->plane;
		brush = brush->Split (plane);		// use backside
		if (!brush) break;					// should not happens
	}

	CBrush::mem = NULL;
	return brush;

	unguardf(("%d", brushNum));
}

#endif


//=============================================================================

cmodel_t *CM_InlineModel (const char *name)
{
	if (!name || name[0] != '*') Com_DropError ("CM_InlineModel: bad name");

	int num = atoi (name+1);
	if (num < 1 || num >= numcmodels) Com_DropError ("CM_InlineModel: bad number");

	return &map_cmodels[num];
}

int CM_NumClusters ()
{
	return numclusters;
}

int CM_NumInlineModels ()
{
	return numcmodels;
}

const char *CM_EntityString ()
{
	return map_entitystring;
}

int CM_LeafContents (int leafnum)
{
	if (leafnum < 0 || leafnum >= numLeafs) Com_DropError ("CM_LeafContents: bad number");
	return map_leafs[leafnum].contents;
}

int CM_LeafCluster (int leafnum)
{
	guard(CM_LeadCluster);
	if (leafnum < 0 || leafnum >= numLeafs) Com_DropError ("CM_LeafCluster: bad number");
	return map_leafs[leafnum].cluster;
	unguard;
}

int CM_LeafArea (int leafnum)
{
	if (leafnum < 0 || leafnum >= numLeafs) Com_DropError ("CM_LeafArea: bad number");
	return map_leafs[leafnum].area;
}


/*-----------------------------------------------------------------------------
	Temporaty box for entity traces
-----------------------------------------------------------------------------*/

static cplane_t	boxPlanes[12];
static int		box_headnode;


// Set up the planes and nodes so that the six floats of a bounding box
// can just be stored out and get a proper clipping hull structure.
static void InitBoxHull ()
{
	box_headnode = numNodes;

	cbrush_t &boxBrush = map_brushes[numBrushes];
	boxBrush.numsides       = 6;
	boxBrush.firstbrushside = numBrushSides;
	boxBrush.contents       = CONTENTS_MONSTER;

	dBsp2Leaf_t &boxLeaf = map_leafs[numLeafs];
	boxLeaf.contents       = CONTENTS_MONSTER;
	boxLeaf.firstleafbrush = numLeafBrushes;
	boxLeaf.numleafbrushes = 1;

	map_leafBrushes[numLeafBrushes] = numBrushes;		// -> boxBrush

	for (int i = 0; i < 6; i++)
	{
		int side = i & 1;

		// brush sides
		cbrushside_t *s = &map_brushSides[numBrushSides+i];
		s->plane   = boxPlanes + i*2+side;
		s->surface = &nullsurface;

		// nodes
		cnode_t *c = &map_nodes[box_headnode+i];
		c->plane = boxPlanes + i*2;
		c->children[side] = -1 - emptyLeaf;				// one child -> empty leaf
		if (i != 5)
			c->children[side^1] = box_headnode+i + 1;	// another child -> next node
		else
			c->children[side^1] = -1 - numLeafs;		// ... or -> boxLeaf

		// planes
		cplane_t *p = &boxPlanes[i*2];
		p->normal.Zero ();
		p->normal[i>>1] = 1;
		p->SetType ();
		p->SetSignbits ();
		p++;											// switch to opposite plane
		p->normal.Zero ();
		p->normal[i>>1] = -1;
		p->SetType ();
		p->SetSignbits ();
	}
}


// To keep everything totally uniform, bounding boxes are turned into small
// BSP trees instead of being compared directly.
int	CM_HeadnodeForBox (const CBox &box)
{
	boxPlanes[0].dist = box.maxs[0];
	boxPlanes[2].dist = box.mins[0];
	boxPlanes[4].dist = box.maxs[1];
	boxPlanes[6].dist = box.mins[1];
	boxPlanes[8].dist = box.maxs[2];
	boxPlanes[10].dist = box.mins[2];

	boxPlanes[1].dist = -box.maxs[0];
	boxPlanes[3].dist = -box.mins[0];
	boxPlanes[5].dist = -box.maxs[1];
	boxPlanes[7].dist = -box.mins[1];
	boxPlanes[9].dist = -box.maxs[2];
	boxPlanes[11].dist = -box.mins[2];

	return box_headnode;
}


/*-----------------------------------------------------------------------------
	Point info
-----------------------------------------------------------------------------*/

int CM_PointLeafnum (const CVec3 &p, int num)
{
	guard(CM_PointLeafnum);
	if (!numNodes) return 0;			// map is not yet loaded
	while (num >= 0)
	{
		cnode_t *node = map_nodes + num;
		float d = node->plane->DistanceTo (p);
		num = node->children[IsNegative(d)];
	}

	c_pointcontents++;					// stats
	unguard;
	return -1 - num;
}


int CM_PointContents (const CVec3 &p, int headnode)
{
	if (!numNodes) return 0;			// map not loaded

	guard(CM_PointContents);
	return map_leafs[CM_PointLeafnum (p, headnode)].contents;
	unguard;
}


/* !! ============================================================================== !!
 * CM_HeadnodeForBox() used with CM_Transformed...(..., angles=(0,0,0)) funcs only
 * angle versions of BoxTrace and PointContents used here only (i.e. with offset,
 * no angles, and HeadnodeForBox)
 * Can remove (combine) this functions, simplify BoxTrace/PointContents for box,
 * but: if will combine later with Q3 engine, it exports to game/client/server
 * functions to create temp box (and capsule?) model with ability to trace it later ...
 * Can (in this case) return from TempBoxModel() special handle, which will be handled
 * by trace functions separately (TempBoxModel() will simply remember box params, and
 * trace functions will use them for fast check)
 * If do so, can remove reserving of 6 planes, 1 brush, leafs, brushsides etc (will reduce
 * memory requirements for map loading - some funcs have copied data with reserving few
 * strucs, and without reserving can simply setup data pointer !)
 */

int	CM_TransformedPointContents (const CVec3 &p, int headnode, const CVec3 &origin, const CVec3 &angles)
{
	guard(CM_TransformedPointContents);

	CVec3	p1;
	if (headnode != box_headnode && (angles[0] || angles[1] || angles[2]))
	{
		CAxis axis;
		axis.FromAngles (angles);
		TransformPoint (origin, axis, p, p1);
	}
	else
		VectorSubtract (p, origin, p1);

	return map_leafs[CM_PointLeafnum (p1, headnode)].contents;

	unguard;
}

int	CM_TransformedPointContents (const CVec3 &p, int headnode, const CVec3 &origin, const CAxis &axis)
{
	guard(CM_TransformedPointContents2);

	CVec3	p1;
	if (headnode != box_headnode)
		TransformPoint (origin, axis, p, p1);
	else
		VectorSubtract (p, origin, p1);

	return map_leafs[CM_PointLeafnum (p1, headnode)].contents;

	unguard;
}


/*-----------------------------------------------------------------------------
	Box info
-----------------------------------------------------------------------------*/

int CM_BoxLeafnums (const CBox &bounds, int *list, int listsize, int *topnode, int headnode)
{
	guard(CM_BoxLeafnums);

	int	stack[MAX_TREE_DEPTH], sptr = 0;		// stack
	int count = 0;
	int _topnode = -1;
	int nodenum = headnode;

	while (true)
	{
		if (nodenum < 0)
		{
			if (count >= listsize) break;		// list is full

			list[count++] = -1 - nodenum;

			if (!sptr)
				break;							// whole tree visited
			else
			{
				nodenum = stack[--sptr];
				continue;
			}
		}

		const cnode_t &node = map_nodes[nodenum];
		switch (BOX_ON_PLANE_SIDE(bounds, *node.plane))
		{
		case 1:
			nodenum = node.children[0];
			break;
		case 2:
			nodenum = node.children[1];
			break;
		default:
			// go down both
			if (_topnode == -1)
				_topnode = nodenum;				// remember top node, which subdivides box
			nodenum = node.children[0];
			stack[sptr++] = node.children[1];
		}
	}

	if (topnode) *topnode = _topnode;
	return count;

	unguard;
}


/*-----------------------------------------------------------------------------
	Box trace
-----------------------------------------------------------------------------*/

#define	DIST_EPSILON	(1.0f/32)

static struct
{
	CVec3	start, end;
	CBox	bounds;
	CVec3	boundsVecs[8];	// corners of bounds
	CVec3	extents;
	trace_t	trace;
	unsigned contents;
	bool	isPoint;
} tr;
static int traceFrame;		// cannot place into "tr": should increment value between frames ("tr" will be zeroed)


// return 'true' when brush clipped trace
static bool ClipBoxToBrush (const cbrush_t &brush)
{
	int		i;
	cbrushside_t *side;

	if (!brush.numsides) return false;

	float enterfrac = -1;
	float leavefrac = 1;					// used only for validating enterfrac
	cplane_t *clipplane = NULL;

	bool	getout, startout;
	getout = startout = false;
	cbrushside_t *leadside = NULL;

	for (i = 0, side = &map_brushSides[brush.firstbrushside]; i < brush.numsides; i++, side++)
	{
		float	d1, d2, dist;

		cplane_t *plane = side->plane;

		if (!tr.isPoint)
		{	// general box case
			if (plane->type <= PLANE_Z)
			{
				// HERE: plane.normal[i] == 0 || 1 for i==type
				dist = plane->dist - tr.bounds.mins[plane->type];
			}
			else if (plane->type <= PLANE_MZ)
			{
				// HERE: plane.normal[i] == 0 || -1 for i==type-3
				dist = plane->dist + tr.bounds.maxs[plane->type-3];
			}
			else
			{
				dist = plane->dist - dot (plane->normal, tr.boundsVecs[plane->signbits]);
			}
		}
		else
		{	// special point case
			dist = plane->dist;
		}

		// this is a 2 plane->DistanceTo() calls with a common plane->type analysis
		if (plane->type <= PLANE_Z)
		{
			d1 = tr.start[plane->type] - dist;
			d2 = tr.end[plane->type] - dist;
		}
		else if (plane->type <= PLANE_MZ)
		{
			d1 = -tr.start[plane->type-3] - dist;
			d2 = -tr.end[plane->type-3] - dist;
		}
		else
		{
			d1 = dot(plane->normal, tr.start) - dist;
			d2 = dot(plane->normal, tr.end) - dist;
		}
if (SHOW_TRACE) RE_DrawTextLeft(va("plane: %g %g %g : %g -- d1=%g d2=%g (%s)",
VECTOR_ARG(plane->normal),plane->dist, d1, d2, side->surface->fullName),RGB(1,0,0));
		// d1 and d2: 0 -- on plane, <0 -- inside brush plane, >0 -- outside brush plane

		if (d1 > 0 && d2 > 0)
		{
			/* math note: here 2 situations
			 * 1) d1 > d2    -- enterfrac will be > 1
			 * 2) d1 < d2    -- leavefrac will be < 0
			 */
if (SHOW_TRACE) RE_DrawTextLeft("d1>0 && d2>0",RGB(0,1,0));
			return false;
		}

		if (d1 <= 0 && d2 <= 0)			// completely in back of this face
			continue;

		if (d1 > 0) startout = true;	// startpoint is not inside this brush
		if (d2 > 0) getout   = true;	// endpoint is not inside this brush

		// crosses face
		float f;
		if (d1 > d2)
		{	// enter
			f = (d1-DIST_EPSILON) / (d1 - d2);
			if (f > enterfrac)
			{
				enterfrac = f;
				clipplane = plane;
				leadside  = side;
			}
		}
		else
		{	// leave
			f = (d1+DIST_EPSILON) / (d1 - d2);
			if (f < leavefrac)
				leavefrac = f;
		}

if (SHOW_TRACE) RE_DrawTextLeft(va("f=%g => ef=%g : lf=%g",f,enterfrac,leavefrac));
		if (enterfrac >= leavefrac)
{
if (SHOW_TRACE) RE_DrawTextLeft("** END: enterfrac < leavefrac",RGB(0.7,0.6,0.5));
			return false;			// intersects (at least) 2 faces outside the brush
}
	}

	if (!startout)
	{
if (SHOW_TRACE) RE_DrawTextLeft("** END: startout",RGB(0.7,0.6,0.5));
		// original point was inside brush
		tr.trace.startsolid = true;
		tr.trace.allsolid   = !getout;
		return true;
	}

if (SHOW_TRACE) RE_DrawTextLeft(va("** END: ef=%g (>-1 & <%g)",enterfrac,tr.trace.fraction),RGB(0.7,0.6,0.5));
	if (enterfrac > -1 && enterfrac < tr.trace.fraction)
	{
		if (enterfrac < 0)			// when startsolid && allsolid
			enterfrac = 0;
		tr.trace.fraction = enterfrac;
		tr.trace.plane    = *clipplane;
		tr.trace.surface  = leadside->surface;
		tr.trace.contents = brush.contents;
if (SHOW_TRACE) RE_DrawTextLeft(va("* best: plane=%g %g %g / %g", VECTOR_ARG(clipplane->normal),clipplane->dist),RGB(1,0.3,0.3));
		return true;
	}

	return false;
}


bool trace_skipAlpha;		//!! need another way to pass through SURF_ALPHA (callbacks??); used in SV_TraceHook()

static void TraceToLeaf (int leafnum)
{
	dBsp2Leaf_t *leaf = &map_leafs[leafnum];
	if (!(leaf->contents & tr.contents))
		return;
	// trace line against all brushes in the leaf
	for (int i = 0; i < leaf->numleafbrushes; i++)
	{
		int brushNum = map_leafBrushes[leaf->firstleafbrush + i];
		cbrush_t &b = map_brushes[brushNum];
if (SHOW_TRACE) RE_DrawTextLeft(va("--- %d cont=%X numSides=%d ---",map_leafBrushes[leaf->firstleafbrush+i], b.contents, b.numsides),RGB(0.3,1,0.3));
		if (b.traceFrame == traceFrame)
			continue;					// already checked this brush in another leaf
		b.traceFrame = traceFrame;
		if (!(b.contents & tr.contents))
			continue;
		if (trace_skipAlpha && b.contents & CONTENTS_ALPHA)
			continue;

		if (ClipBoxToBrush (b))
			tr.trace.brushNum = brushNum;
		if (!tr.trace.fraction)		// when startsolid && allsolid
			return;
	}
}


/*
================
TestBoxInBrush

if box inside this brush or intersects its brushsides:
 1) trace->fraction=0
 2) trace->startsolid = true
 3) trace->allsolid = true
else trace is unchanged
================
*/
static bool TestBoxInBrush (const cbrush_t &brush)
{
	if (!brush.numsides)
		return false;

	float clipdist = -BIG_NUMBER;
	cbrushside_t *clipside = NULL;
	int i;
	cbrushside_t *side;
	for (i = 0, side = &map_brushSides[brush.firstbrushside]; i < brush.numsides; i++, side++)
	{
		cplane_t *plane = side->plane;

		float size;
		if (plane->type <= PLANE_Z)
			size = - tr.bounds.mins[plane->type];
		else if (plane->type <= PLANE_MZ)
			size = tr.bounds.maxs[plane->type-3];
		else
			size = - dot(plane->normal, tr.boundsVecs[plane->signbits]);

		float d = plane->DistanceTo (tr.start) - size;
		// if completely in front of face, no intersection
		if (d > 0) return false;
		if (d > clipdist)
		{
			clipdist = d;
			clipside = side;
		}
	}

	// inside this brush
	tr.trace.startsolid = tr.trace.allsolid = true;
	tr.trace.fraction = 0;
	tr.trace.contents = brush.contents;
	tr.trace.plane    = *clipside->plane;
	tr.trace.surface  = clipside->surface;

	return true;
}


// Call TestBoxInBrush for each leafbrush.
// When first intersection found (trace.fraction == 0) - return
static void TestInLeaf (int leafnum)
{
	dBsp2Leaf_t *leaf = &map_leafs[leafnum];
	if (!(leaf->contents & tr.contents))
		return;

	// trace line against all brushes in the leaf
	for (int i = 0; i < leaf->numleafbrushes; i++)
	{
		int brushNum = map_leafBrushes[leaf->firstleafbrush + i];
		cbrush_t &b = map_brushes[brushNum];
		if (b.traceFrame == traceFrame)
			continue;	// already checked this brush in another leaf
		b.traceFrame = traceFrame;

		if (!(b.contents & tr.contents))
			continue;
		if (TestBoxInBrush (b))
		{
			tr.trace.brushNum = brushNum;
			return;
		}
	}
}


/*-----------------------------------------------------------------------------
	Common trace code
-----------------------------------------------------------------------------*/

static void RecursiveHullCheck (int nodeNum, float p1f, float p2f, const CVec3 &point1, const CVec3 &point2)
{
	CVec3 p1 = point1;
	CVec3 p2 = point2;

	if (tr.trace.fraction <= p1f)
		return;		// already hit something nearer (??)

	while (true)
	{
		// if < 0, we are in a leaf node
		if (nodeNum < 0)
		{
			TraceToLeaf (-1-nodeNum);
			return;
		}

		// find the point distances to the separating plane
		// and the offset for the size of the box
		const cnode_t &node = map_nodes[nodeNum];
		const cplane_t &plane = *node.plane;

		float t1, t2, offset;
		if (plane.type <= PLANE_Z)
		{
			t1 = p1[plane.type] - plane.dist;
			t2 = p2[plane.type] - plane.dist;
			offset = tr.extents[plane.type];
		}
		else if (plane.type <= PLANE_MZ)
		{
			t1 = -p1[plane.type-3] - plane.dist;
			t2 = -p2[plane.type-3] - plane.dist;
			offset = tr.extents[plane.type-3];
		}
		else
		{
			t1 = dot (plane.normal, p1) - plane.dist;
			t2 = dot (plane.normal, p2) - plane.dist;
			if (tr.isPoint)
				offset = 0;
			else
				offset = fabs (tr.extents[0]*plane.normal[0]) +
						 fabs (tr.extents[1]*plane.normal[1]) +
						 fabs (tr.extents[2]*plane.normal[2]);
		}

		// see which sides we need to consider
		if (t1 >= offset && t2 >= offset)
		{
			nodeNum = node.children[0];
			continue;
		}
		if (t1 < -offset && t2 < -offset)
		{
			nodeNum = node.children[1];
			continue;
		}

		int side;
		float frac1, frac2;
		// put the crosspoint DIST_EPSILON pixels on the near side
		if (t1 == t2)
		{
			frac1 = 1;
			frac2 = 0;
			side  = 0;
		}
		else
		{
			float idist = 1.0f / (t1 - t2);
			offset += DIST_EPSILON;
#if 0
			if (t1 < t2)						// NOTE: here idist<0
			{
				frac1 = (t1 - offset) * idist;	// -offset*idist > 0
				frac2 = (t1 + offset) * idist;	// +offset*idist < 0
				side  = 1;
			}
			else if (t1 > t2)					// here idist>0
			{
				frac1 = (t1 + offset) * idist;	// +offset*idist > 0
				frac2 = (t1 - offset) * idist;	// -offset*idist < 0
				side  = 0;
			}
#else
			frac1 = frac2 = t1 * idist;
			float offset2;
			FAbsSign (idist, offset2, side);	// side=1 when t1<t2
			offset2 *= offset;					// offset2 = fabs(idist)*offset; >0
			frac1 += offset2;
			frac2 -= offset2;
#endif

			frac1 = bound(frac1, 0, 1);
			frac2 = bound(frac2, 0, 1);
		}

		float midf;
		CVec3 mid;

		// move up to the node
		midf = Lerp (p1f, p2f, frac1);
		Lerp (p1, p2, frac1, mid);
		RecursiveHullCheck (node.children[side], p1f, midf, p1, mid);

		// go past the node
		midf = Lerp (p1f, p2f, frac2);
#if 0
		Lerp (p1, p2, frac2, mid);
		RecursiveHullCheck (node.children[side^1], midf, p2f, mid, p2);
#else
		Lerp (p1, p2, frac2, p1);
		// do "RecursiveHullCheck (node.children[side^1], midf, p2f, mid, p2);"
		p1f = midf;
		nodeNum = node.children[side^1];
#endif
	}
}


/*
==================
CM_BoxTrace

When start==end:
  1) trace.contents is set (only this field contains information!)
  2) trace.fraction = 0
  3) trace.endpos = start
  4) trace.allsolid = trace.startsolid = true
  5) trace.surface = nullsurface
  6) trace.ent & trace.plane are unchanged
==================
*/
//?? check: can this be faster if trace with sphere
void CM_BoxTrace (trace_t &trace, const CVec3 &start, const CVec3 &end, const CBox &bounds, int headnode, int brushmask)
{
	guard(CM_BoxTrace);

	traceFrame++;	// to avoid testing single brush from different leafs
	c_traces++;		//??

	// fill in a default trace
	memset (&tr, 0, sizeof(tr));
	tr.trace.fraction = 1;
	tr.trace.surface  = &nullsurface;
	tr.trace.brushNum = -1;

	if (!numNodes)	// map not loaded
	{
		trace = tr.trace;
		return;
	}

	tr.contents = brushmask;
	tr.start    = start;
	tr.end      = end;
	tr.bounds   = bounds;
	for (int boundsVec = 0; boundsVec < 8; boundsVec++)
	{
		int signbits = boundsVec;
		for (int j = 0; j < 3; j++, signbits >>= 1)
			tr.boundsVecs[boundsVec][j] = (signbits & 1) ? bounds.maxs[j] : bounds.mins[j];
	}

	// check for "position test" special case
	if (start[0] == end[0] && start[1] == end[1] && start[2] == end[2])
	{
		CBox box;
		int i;
		for (i = 0; i < 3; i++)
		{
			box.mins[i] = start[i] + tr.bounds.mins[i] - 1;
			box.maxs[i] = start[i] + tr.bounds.maxs[i] + 1;
		}

		int leafs[1024];
		int numLeafs = CM_BoxLeafnums (box, ARRAY_ARG(leafs), NULL, headnode);
		for (i = 0; i < numLeafs; i++)
		{
			TestInLeaf (leafs[i]);
			if (tr.trace.allsolid)	// always set when 1st intersection by CM_TestBoxInBrush()
				break;
		}
		tr.trace.endpos = start;
		trace = tr.trace;
		return;
	}

	// check for "point" special case
	if (tr.bounds.mins[0] == 0 && tr.bounds.mins[1] == 0 && tr.bounds.mins[2] == 0 &&
		tr.bounds.maxs[0] == 0 && tr.bounds.maxs[1] == 0 && tr.bounds.maxs[2] == 0)
		tr.isPoint = true;
	else
	{
		tr.extents[0] = -tr.bounds.mins[0] > tr.bounds.maxs[0] ? -tr.bounds.mins[0] : tr.bounds.maxs[0];
		tr.extents[1] = -tr.bounds.mins[1] > tr.bounds.maxs[1] ? -tr.bounds.mins[1] : tr.bounds.maxs[1];
		tr.extents[2] = -tr.bounds.mins[2] > tr.bounds.maxs[2] ? -tr.bounds.mins[2] : tr.bounds.maxs[2];
	}

	// general sweeping through world
	RecursiveHullCheck (headnode, 0, 1, start, end);

	if (tr.trace.fraction == 1)
		tr.trace.endpos = end;
	else
		Lerp (start, end, tr.trace.fraction, tr.trace.endpos);

	trace = tr.trace;
	return;
	unguard;
}


// Handles offseting and rotation of the end points for moving and rotating entities
void CM_TransformedBoxTrace (trace_t &trace, const CVec3 &start, const CVec3 &end, const CBox &bounds,
	int headnode, int brushmask, const CVec3 &origin, const CVec3 &angles)
{
	guard(CM_TransformedBoxTrace);

	bool	rotated;
	// rotate start and end into the models frame of reference
	if (headnode != box_headnode && (angles[0] || angles[1] || angles[2]))
		rotated = true;
	else
		rotated = false;

	CVec3	start1, end1;
	CAxis	axis;
	if (rotated)
	{
		axis.FromAngles (angles);
		// transform start/end to axis (model coordinate system)
		TransformPoint (origin, axis, start, start1);
		TransformPoint (origin, axis, end, end1);
	}
	else
	{
		VectorSubtract (start, origin, start1);
		VectorSubtract (end, origin, end1);
	}

	// sweep the box through the model
	CM_BoxTrace (trace, start1, end1, bounds, headnode, brushmask);

	// transform normal/endpos back to world coordinate system
	if (trace.fraction != 1.0f && rotated)
		axis.UnTransformVector (trace.plane.normal, trace.plane.normal);
	if (rotated)
		UnTransformPoint (origin, axis, trace.endpos, trace.endpos);
	else
		trace.endpos.Add (origin);

	unguard;
}


//?? use CCoords instead of origin+axis
void CM_TransformedBoxTrace (trace_t &trace, const CVec3 &start, const CVec3 &end, const CBox &bounds,
	int headnode, int brushmask, const CVec3 &origin, const CAxis &axis)
{
	guard(CM_TransformedBoxTrace2);

	CVec3 start1, end1;
	// transform start/end to axis (model coordinate system)
	TransformPoint (origin, axis, start, start1);
	TransformPoint (origin, axis, end, end1);

	// sweep the box through the model
	CM_BoxTrace (trace, start1, end1, bounds, headnode, brushmask);

	// transform normal/endpos to world coordinate system
	if (trace.fraction != 1.0f)
		axis.UnTransformVector (trace.plane.normal, trace.plane.normal);
	UnTransformPoint (origin, axis, trace.endpos, trace.endpos);

	unguard;
}


/*-----------------------------------------------------------------------------
	Occlusion test
-----------------------------------------------------------------------------*/

static int trace_numBrushes, trace_maxBrushes;
static int *trace_brushes;


// returns true when start->end line intersects brush
static bool TestBrush (const CVec3 &start, const CVec3 &end, const cbrush_t &brush)
{
	int		i;
	cbrushside_t *side;

	float ef = -1;					// enterfrac
	float lf =  1;					// leavefrac
	for (i = 0, side = &map_brushSides[brush.firstbrushside]; i < brush.numsides; i++, side++)
	{
		float	d1, d2, f;

		cplane_t *plane = side->plane;
		if (plane->type <= PLANE_Z)
		{
			d1 = start[plane->type] - plane->dist;
			d2 = end[plane->type] - plane->dist;
		}
		else if (plane->type <= PLANE_MZ)
		{
			d1 = -start[plane->type-3] - plane->dist;
			d2 = -end[plane->type-3] - plane->dist;
		}
		else
		{
			d1 = dot (plane->normal, start) - plane->dist;
			d2 = dot (plane->normal, end) - plane->dist;
		}

		// d1 and d2: 0 -- on plane, <0 -- inside brush plane, >0 -- outside brush plane
		if (d1 >= 0 && d2 >= 0)		// start and end points are outside this plane
			return false;
		if (d1 < 0 && d2 < 0)		// both points inside plane
			continue;

		// check for situation, when line crosses 2 planes outside brush (in this case,
		// line will leave brush before enter -- "inside-out")
		// here: d1 != d1 (have different signs)
		f = d1 / (d1 - d2);
		if (d1 > d2)
		{
			if (f > ef) ef = f;
		}
		else
		{
			if (f < lf) lf = f;
		}
		if (ef > lf) return false;
	}
	return true;
}


static void RecursiveBrushTest (const CVec3 &point1, const CVec3 &point2, int nodeNum)
{
	CVec3 p1 = point1;
	CVec3 p2 = point2;
	while (trace_numBrushes < trace_maxBrushes)					// have buffer space
	{
		if (nodeNum < 0)
		{
			//------------------ test leaf ---------------------
			dBsp2Leaf_t &leaf = map_leafs[-1-nodeNum];
			if (!(leaf.contents & CONTENTS_SOLID)) return;		// we are checking only SOLID leafs

			for (int i = 0; i < leaf.numleafbrushes; i++)
			{
				//-------------- test brush --------------------
				int brushNum = map_leafBrushes[leaf.firstleafbrush + i];
				cbrush_t &b = map_brushes[brushNum];
				if (b.traceFrame == traceFrame || !(b.contents & CONTENTS_SOLID))
					continue;									// already checked, or non-solid
				b.traceFrame = traceFrame;
				if (TestBrush (p1, p2, b))
				{
					// remember brush
					trace_brushes[trace_numBrushes++] = brushNum;
					if (trace_numBrushes >= trace_maxBrushes)
						return;									// buffer full
				}
			}
			return;
		}

		const cnode_t &node = map_nodes[nodeNum];
		const cplane_t &plane = *node.plane;

		float t1, t2;
		if (plane.type <= PLANE_Z)
		{
			t1 = p1[plane.type] - plane.dist;
			t2 = p2[plane.type] - plane.dist;
		}
		else if (plane.type <= PLANE_MZ)
		{
			t1 = -p1[plane.type-3] - plane.dist;
			t2 = -p2[plane.type-3] - plane.dist;
		}
		else
		{
			t1 = dot(plane.normal, p1) - plane.dist;
			t2 = dot(plane.normal, p2) - plane.dist;
		}

		int s1, s2;
		s1 = IsNegative (t1); s2 = IsNegative (t2);
		if (s1 == s2)
		{
			nodeNum = node.children[s1];	// t1 >= 0  => [0], < 0  => [1]
			continue;
		}

		// here: sign(t1) != sign(t2), t1-t2 != 0
		float idist = 1.0f / (t1 - t2);
		float frac1, frac2;
		int side;
#if 0
		if (t1 < t2)						// t1<0, t2>=0; here idist<0
		{
			frac1 = (t1 - DIST_EPSILON) * idist;
			frac2 = (t1 + DIST_EPSILON) * idist;
			side  = 1;
		}
		else								// t1>=0, t2<0
		{
			frac1 = (t1 + DIST_EPSILON) * idist;
			frac2 = (t1 - DIST_EPSILON) * idist;
			side  = 0;
		}
#else
		frac1 = frac2 = t1 * idist;
		float offset2;
		FAbsSign (idist, offset2, side);	// side=1 when t1<t2 (when t1<0 and t2>0)
		offset2 *= DIST_EPSILON;			// offset2 = fabs(idist)*DIST_EPSILON; >0
		frac1 += offset2;
		frac2 -= offset2;
#endif
		frac1 = bound(frac1, 0, 1);
		frac2 = bound(frac2, 0, 1);

		CVec3 mid;
		// move up to the node
		Lerp (p1, p2, frac1, mid);

		RecursiveBrushTest (p1, mid, node.children[side]);

		// go past the node
		Lerp (p1, p2, frac2, p1);

		// do "RecursiveBrushTest (start, end, node.children[side^1]);"
		nodeNum = node.children[side^1];
	}
}


// returns number of brushes, intersected with line
int CM_BrushTrace (const CVec3 &start, const CVec3 &end, int *brushes, int maxBrushes)
{
	guard(CM_BrushTrace);

	trace_numBrushes = 0;
	trace_brushes    = brushes;
	trace_maxBrushes = maxBrushes;
	traceFrame++;

	RecursiveBrushTest (start, end, 0);
	return trace_numBrushes;

	unguard;
}


// gets list of brushes (taken from CM_BrushTrace() call) and removes brushes,
// which are not intersected with line "start-end"
int CM_RefineBrushTrace (const CVec3 &start, const CVec3 &end, int *brushes, int numBrushes)
{
	guard(CM_RefineBrushTrace);

	int *src, *dst;
	src = dst = brushes;
	int newNum = 0;
	for (int i = 0; i < numBrushes; i++)
	{
		int b = *src++;
		if (TestBrush (start, end, map_brushes[b]))
		{
			*dst++ = b;
			newNum++;
		}
	}

	return newNum;
	unguard;
}


/*-----------------------------------------------------------------------------
	PVS support
-----------------------------------------------------------------------------*/

const byte *CM_ClusterPVS (int cluster)
{
	guard(CM_ClusterPVS);

	static byte	pvsrow[MAX_MAP_LEAFS/8];
	if (cluster <= -1)
	{
		if (pvsrow[0] != 0)
			memset (pvsrow, 0, (numclusters + 7) >> 3);
		return pvsrow;
	}
	if (!map_bspfile->visInfo)
	{
		// only 1 cluster per whole map
		static const byte noVisPVS[] = { 255, 255 };	// one byte enough ...
		return noVisPVS;
	}

	return map_bspfile->visInfo + cluster * map_bspfile->visRowSize;
	unguard;
}


/*-----------------------------------------------------------------------------
	Areaportals
-----------------------------------------------------------------------------*/

static void FloodArea_r (carea_t *area, int floodnum)
{
	if (area->floodvalid == floodvalid)
	{
		if (area->floodnum == floodnum)
			return;
		Com_DropError ("FloodArea_r: reflooded");
	}

	area->floodnum   = floodnum;
	area->floodvalid = floodvalid;
	dareaportal_t *p = &map_areaportals[area->firstareaportal];
	for (int i = 0; i < area->numareaportals; i++, p++)
	{
		if (portalopen[p->portalnum])
			FloodArea_r (&map_areas[p->otherarea], floodnum);
	}
}

static void FloodAreaConnections ()
{
	// all current floods are now invalid
	floodvalid++;
	int floodnum = 0;

	// area 0 is not used
	for (int i = 1; i < numAreas; i++)
	{
		carea_t *area = &map_areas[i];
		if (area->floodvalid == floodvalid)
			continue;		// already flooded into
		floodnum++;
		FloodArea_r (area, floodnum);
	}

}

void CM_SetAreaPortalState (int portalnum, bool open)
{
	if (portalnum > numareaportals)
		Com_DropError ("areaportal > numareaportals");

	portalopen[portalnum] = open;
	FloodAreaConnections ();
}

// used by game dll
bool CM_AreasConnected (int area1, int area2)
{
	if (map_noareas->integer)
		return true;

	if (area1 > numAreas || area2 > numAreas)
		Com_DropError ("area > numAreas");

	if (map_areas[area1].floodnum == map_areas[area2].floodnum)
		return true;
	return false;
}


/*
=================
CM_WriteAreaBits

Writes a length byte followed by a bit vector of all the areas
that area in the same flood as the area parameter

This is used by the renderer to cull visibility
=================
*/
int CM_WriteAreaBits (byte *buffer, int area)
{
	int bytes = (numAreas + 7) >> 3;

	if (map_noareas->integer)
	{	// for debugging, send everything
		memset (buffer, 255, bytes);
	}
	else
	{
		memset (buffer, 0, bytes);

		int floodnum = map_areas[area].floodnum;
		for (int i = 0; i < numAreas; i++)
		{
			if (map_areas[i].floodnum == floodnum || !area)
				buffer[i >> 3] |= 1 << (i & 7);
		}
	}

	return bytes;
}


// Writes the portal state to a savegame file
void CM_WritePortalState (FILE *f)
{
	fwrite (portalopen, sizeof(portalopen), 1, f);
}


// Reads the portal state from a savegame file and recalculates the area connections
void CM_ReadPortalState (FILE *f)
{
	fread (portalopen, sizeof(portalopen), 1, f);
	FloodAreaConnections ();
}

/*
=============
CM_HeadnodeVisible

Returns true if any leaf under headnode has a cluster that
is potentially visible
=============
*/
// This function used only from sv_ents.c :: SV_BuildClientFrame()
bool CM_HeadnodeVisible (int nodenum, const byte *visbits)
{
	guard(CM_HeadnodeVisible);

	int stack[MAX_TREE_DEPTH], sptr = 0;	// stack
	while (true)
	{
		if (nodenum < 0)
		{
			int cluster = map_leafs[-1-nodenum].cluster;
			if (cluster == -1 || !(visbits[cluster>>3] & (1<<(cluster&7))))
			{
				if (!sptr) return false;	// whole tree visited
				nodenum = stack[--sptr];
				continue;
			}
			return true;
		}

		cnode_t *node = &map_nodes[nodenum];
		stack[sptr++] = node->children[0];
		nodenum = node->children[1];
	}

	unguard;
}
