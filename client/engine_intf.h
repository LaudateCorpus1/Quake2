// Autogenerated file: do not modify

struct refImport_t
{
	int		struc_size;
	cvar_t*	(*_Cvar_Get) (const char *name, const char *value, int flags);
	void	(*_Cvar_GetVars) (const cvarInfo_t *vars, int count);
	cvar_t*	(*_Cvar_Set) (const char *name, const char *value);
	cvar_t*	(*_Cvar_SetValue) (const char *name, float value);
	cvar_t*	(*_Cvar_SetInteger) (const char *name, int value);
	const char*	(*_Cvar_VariableString) (const char *name);
	float	(*_Cvar_VariableValue) (const char *name);
	int	(*_Cvar_VariableInt) (const char *name);
	float	(*_Cvar_Clamp) (cvar_t *cvar, float low, float high);
	void	(*_Com_DPrintf) (const char *str, ...) PRINTF(1,2);
	const char*	(*_FS_Gamedir) ();
	void	(*_FS_CopyFile) (const char *src, const char *dst);
	void	(*_FS_CopyFiles) (const char *srcMask, const char *dstDir);
	void	(*_FS_RemoveFiles) (const char *mask);
	void	(*_Vid_Restart) ();
	bool	(*_Vid_GetModeInfo) (int *width, int *height, int mode);
	int	(*_ImageExists) (const char *name, int stop_mask);
	byte*	(*_LoadPCX) (const char *name, int &width, int &height, byte *&palette);
	byte*	(*_LoadTGA) (const char *name, int &width, int &height);
	byte*	(*_LoadJPG) (const char *name, int &width, int &height);
	bool	(*_WriteTGA) (const char *name, byte *pic, int width, int height);
	bool	(*_WriteJPG) (const char *name, byte *pic, int width, int height, bool highQuality);
	bspfile_t*	(*_LoadBspFile) (const char *filename, bool clientload, unsigned *checksum);
	void	(*_CM_BoxTrace) (trace_t &tr, const CVec3 &start, const CVec3 &end, const CBox &bounds, int headnode, unsigned contents);
	void	(*_CM_TransformedBoxTrace) (trace_t &tr, const CVec3 &start, const CVec3 &end, const CBox &bounds, int headnode, unsigned contents, const CVec3 &origin, const CVec3 &angles);
	void	(*_CM_TransformedBoxTrace1) (trace_t &tr, const CVec3 &start, const CVec3 &end, const CBox &bounds, int headnode, unsigned contents, const CVec3 &origin, const CAxis &axis);
	void	(*_CM_ClipTraceToModels) (trace_t &trace, const CVec3 &start, const CVec3 &end, const CBox &bounds, unsigned contents);
	int	(*_CM_BrushTrace) (const CVec3 &start, const CVec3 &end, int *brushes, int maxBrushes);
	int	(*_CM_RefineBrushTrace) (const CVec3 &start, const CVec3 &end, int *brushes, int numBrushes);
#if _WIN32
	void*	(*_Vid_CreateWindow) (int width, int height, bool fullscreen);
	void	(*_Vid_DestroyWindow) (bool force);
#endif
};

#if STATIC_BUILD || !IS_RENDERER
// engine itself or statically linked renderer
#include "engine.h"
#else // dynamic renderer -> engine interface

extern refImport_t ri;

inline cvar_t* Cvar_Get (const char *name, const char *value, int flags = 0)
{
	return ri._Cvar_Get (name, value, flags);
}
inline void Cvar_GetVars (const cvarInfo_t *vars, int count)
{
	ri._Cvar_GetVars (vars, count);
}
inline cvar_t* Cvar_Set (const char *name, const char *value)
{
	return ri._Cvar_Set (name, value);
}
inline cvar_t* Cvar_SetValue (const char *name, float value)
{
	return ri._Cvar_SetValue (name, value);
}
inline cvar_t* Cvar_SetInteger (const char *name, int value)
{
	return ri._Cvar_SetInteger (name, value);
}
inline const char* Cvar_VariableString (const char *name)
{
	return ri._Cvar_VariableString (name);
}
inline float Cvar_VariableValue (const char *name)
{
	return ri._Cvar_VariableValue (name);
}
inline int Cvar_VariableInt (const char *name)
{
	return ri._Cvar_VariableInt (name);
}
inline float Cvar_Clamp (cvar_t *cvar, float low, float high)
{
	return ri._Cvar_Clamp (cvar, low, high);
}
inline void Com_DPrintf (const char *str, ...) PRINTF(1,2)
{
	ri._Com_DPrintf (str);
}
inline const char* FS_Gamedir ()
{
	return ri._FS_Gamedir ();
}
inline void FS_CopyFile (const char *src, const char *dst)
{
	ri._FS_CopyFile (src, dst);
}
inline void FS_CopyFiles (const char *srcMask, const char *dstDir)
{
	ri._FS_CopyFiles (srcMask, dstDir);
}
inline void FS_RemoveFiles (const char *mask)
{
	ri._FS_RemoveFiles (mask);
}
inline void Vid_Restart ()
{
	ri._Vid_Restart ();
}
inline bool Vid_GetModeInfo (int *width, int *height, int mode)
{
	return ri._Vid_GetModeInfo (width, height, mode);
}
inline int ImageExists (const char *name, int stop_mask = IMAGE_ANY)
{
	return ri._ImageExists (name, stop_mask);
}
inline byte* LoadPCX (const char *name, int &width, int &height, byte *&palette)
{
	return ri._LoadPCX (name, width, height, palette);
}
inline byte* LoadTGA (const char *name, int &width, int &height)
{
	return ri._LoadTGA (name, width, height);
}
inline byte* LoadJPG (const char *name, int &width, int &height)
{
	return ri._LoadJPG (name, width, height);
}
inline bool WriteTGA (const char *name, byte *pic, int width, int height)
{
	return ri._WriteTGA (name, pic, width, height);
}
inline bool WriteJPG (const char *name, byte *pic, int width, int height, bool highQuality)
{
	return ri._WriteJPG (name, pic, width, height, highQuality);
}
inline bspfile_t* LoadBspFile (const char *filename, bool clientload, unsigned *checksum)
{
	return ri._LoadBspFile (filename, clientload, checksum);
}
inline void CM_BoxTrace (trace_t &tr, const CVec3 &start, const CVec3 &end, const CBox &bounds, int headnode, unsigned contents)
{
	ri._CM_BoxTrace (tr, start, end, bounds, headnode, contents);
}
inline void CM_TransformedBoxTrace (trace_t &tr, const CVec3 &start, const CVec3 &end, const CBox &bounds, int headnode, unsigned contents, const CVec3 &origin, const CVec3 &angles)
{
	ri._CM_TransformedBoxTrace (tr, start, end, bounds, headnode, contents, origin, angles);
}
inline void CM_TransformedBoxTrace (trace_t &tr, const CVec3 &start, const CVec3 &end, const CBox &bounds, int headnode, unsigned contents, const CVec3 &origin, const CAxis &axis)
{
	ri._CM_TransformedBoxTrace1 (tr, start, end, bounds, headnode, contents, origin, axis);
}
inline void CM_ClipTraceToModels (trace_t &trace, const CVec3 &start, const CVec3 &end, const CBox &bounds, unsigned contents)
{
	ri._CM_ClipTraceToModels (trace, start, end, bounds, contents);
}
inline int CM_BrushTrace (const CVec3 &start, const CVec3 &end, int *brushes, int maxBrushes)
{
	return ri._CM_BrushTrace (start, end, brushes, maxBrushes);
}
inline int CM_RefineBrushTrace (const CVec3 &start, const CVec3 &end, int *brushes, int numBrushes)
{
	return ri._CM_RefineBrushTrace (start, end, brushes, numBrushes);
}
#if _WIN32
inline void* Vid_CreateWindow (int width, int height, bool fullscreen)
{
	return ri._Vid_CreateWindow (width, height, fullscreen);
}
inline void Vid_DestroyWindow (bool force)
{
	ri._Vid_DestroyWindow (force);
}
#endif

#endif // renderer -> engine interface

