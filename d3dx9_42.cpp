#include <Windows.h>
#include <Shlwapi.h>
#include <tchar.h>

#include <vector>
#include <fstream>

#include "detours/Detours.h"

HINSTANCE hLThis = 0;
FARPROC p[329];
HINSTANCE hL = 0;
bool loadedPlugins = false;
std::string logName = "d3dx9_42.log";

PVOID origFunc = 0;

std::string skyrimPath;
bool alreadyLoaded = false;
std::vector<HINSTANCE> loadedLib;

int LoadDLLPlugin(const char * path)
{
	int state = -1;
	__try
	{
		HINSTANCE lib = LoadLibrary(path);
		if (lib == NULL)
			return 0;

		int ok = 1;
		FARPROC funcAddr = GetProcAddress(lib, "Initialize");
		if (funcAddr != 0)
		{
			state = -2;
			((void(__cdecl *)())funcAddr)();
			ok = 2;
		}

		loadedLib.push_back(lib);
		return ok;
	}
	__except (1)
	{

	}

	return state;
}

std::string GetPluginsDirectory()
{
	std::string pluginPath = skyrimPath;
	auto pos = pluginPath.rfind('\\');
	if (pos != std::string::npos)
		pluginPath = pluginPath.substr(0, pos);

	return pluginPath + "\\Data\\SKSE\\Plugins\\";
}

std::string GetCKPluginsDirectory()
{
	std::string pluginPath = skyrimPath;
	auto pos = pluginPath.rfind('\\');
	if (pos != std::string::npos)
		pluginPath = pluginPath.substr(0, pos);

	return pluginPath + "\\CKPlugins\\";
}

void LoadCKPlugins()
{
	if (alreadyLoaded)
		return;

	alreadyLoaded = true;

	std::ofstream logFile(logName, std::ios_base::out | std::ios_base::app);

	WIN32_FIND_DATA wfd;
	std::string dir = GetCKPluginsDirectory();
	std::string search_dlls = dir + "*.dll";
	HANDLE hFind = FindFirstFile(search_dlls.c_str(), &wfd);

	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			char plugin_path[MAX_PATH];

			_snprintf_s(plugin_path, MAX_PATH, "%s%s.dll", dir.c_str(), wfd.cFileName);

			logFile << "attempting to load " << plugin_path << std::endl;

			int result = LoadDLLPlugin(plugin_path);

			switch (result)
			{
			case 2:
				logFile << "loaded successfully and Initialize() called" << std::endl;
				break;
			case 1:
				logFile << "loaded successfully" << std::endl;
				break;
			case 0:
				logFile << "LoadLibrary failed" << std::endl;
				break;
			case -1:
				logFile << "LoadLibrary crashed, contact the plugin author" << std::endl;
				break;
			case -2:
				logFile << "Initialize() crashed, contact the plugin author" << std::endl;
				break;
			}

		} while (FindNextFile(hFind, &wfd));
		FindClose(hFind);
	}
	else
	{
		logFile << "failed to search plugin directory" << std::endl;
	}

	logFile << "loader finished" << std::endl;
}

void LoadSKSEPlugins()
{
	if (alreadyLoaded)
		return;

	alreadyLoaded = true;

	std::ofstream logFile(logName, std::ios_base::out | std::ios_base::app);

	logFile << "hook triggered, loading skse plugins" << std::endl;

	std::vector<std::string> filesToLoad;
	WIN32_FIND_DATA wfd;
	std::string dir = GetPluginsDirectory();
	std::string search_dir = dir + "*.txt";
	HANDLE hFind = FindFirstFile(search_dir.c_str(), &wfd);
	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			if ((wfd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
				continue;

			std::string fileName = wfd.cFileName;

			if (fileName.length() < 4)
				continue;

			std::string ext = fileName.substr(fileName.length() - 4, 4);

			if (_stricmp(ext.c_str(), ".txt") != 0)
				continue;

			fileName = fileName.substr(0, fileName.length() - 4);

			if (fileName.length() < 8)
				continue;

			std::string end = fileName.substr(fileName.length() - 8, 8);

			if (_stricmp(end.c_str(), "_preload") != 0)
				continue;

			fileName = fileName.substr(0, fileName.length() - 8);

			filesToLoad.push_back(fileName);

			logFile << "found plugin \"" << fileName.c_str() << "\" for preloading" << std::endl;

		} while (FindNextFile(hFind, &wfd));
		FindClose(hFind);
	}
	else
	{
		logFile << "failed to search skse plugin directory" << std::endl;
	}

	logFile << "attempting to load found plugins" << std::endl;

	char plugin_path[MAX_PATH];

	for (auto file : filesToLoad)
	{
		_snprintf_s(plugin_path, MAX_PATH, "%s%s.dll", dir.c_str(), file.c_str());

		logFile << "attempting to load \"" << plugin_path << "\"" << std::endl;

		int result = LoadDLLPlugin(plugin_path);

		switch(result)
		{
		case 2:
			logFile << "loaded successfully and Initialize() called" << std::endl;
			break;
		case 1:
			logFile << "loaded successfully" << std::endl;
			break;
		case 0:
			logFile << "LoadLibrary failed" << std::endl;
			break;
		case -1:
			logFile << "LoadLibrary crashed, contact the plugin author" << std::endl;
			break;
		case -2:
			logFile << "Initialize() crashed, contact the plugin author" << std::endl;
			break;
		}
	}

	logFile << "loader finished" << std::endl;
}

PVOID hookedFunc(PVOID arg1, PVOID arg2)
{
	LoadSKSEPlugins();
	return ((PVOID(*)(PVOID, PVOID))origFunc)(arg1, arg2);
}

BOOL WINAPI DllMain(HINSTANCE hInst,DWORD reason,LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		std::ofstream logFile(logName);

		logFile << "skse64 plugin preloader - d3dx9_42" << std::endl;

		hLThis = hInst;

		/*
		 * Check if we're loaded by SkyrimSE.exe
		 * Purpose two-fold:
		 *	1) Don't load skse plugins into things that aren't Skyrim (Creation Kit)
		 *	2) SkyrimSE uses a very limited subset of d3dx9_42, with implementations provided in d3dx9_impl, so we don't need to load the actual dll in this case
		 */
		TCHAR exePath[MAX_PATH];
		GetModuleFileName(nullptr, exePath, MAX_PATH);

		TCHAR * exeName = PathFindFileName(exePath);

		logFile << "exe path: " << exePath << std::endl;

		skyrimPath = exePath;

		if (_tcscmp(exeName, _T("SkyrimSE.exe")) == 0)
		{
			logFile << "loaded into SkyrimSE.exe, proxying SkyrimSE d3dx9_42 funcs and registering preload hook" << std::endl;
			loadedPlugins = true;

			// delay plugin preload so MO VFS has a chance to attach, otherwise loader won't see inside VFS
			auto moduleBase = reinterpret_cast<uintptr_t>(GetModuleHandle(nullptr));

			origFunc = Detours::IATHook((PBYTE)moduleBase, "api-ms-win-crt-runtime-l1-1-0.dll", "_initterm_e", (PBYTE)hookedFunc);

			logFile << "success" << std::endl;
		}
		else
		{
			logFile << "loaded into a non-SkyrimSE exe, proxying d3dx9_42 but not registering hooks" << std::endl;

			TCHAR dllPath[MAX_PATH];
			if (!GetSystemDirectory(dllPath, MAX_PATH))
				return false;

			if (_tcscat_s(dllPath, MAX_PATH, _T("\\d3dx9_42.dll")) != 0)
				return false;

			hL = LoadLibrary(dllPath);
			if (!hL) return false;

			p[0] = GetProcAddress(hL, "D3DXAssembleShader");
			p[1] = GetProcAddress(hL, "D3DXAssembleShaderFromFileA");
			p[2] = GetProcAddress(hL, "D3DXAssembleShaderFromFileW");
			p[3] = GetProcAddress(hL, "D3DXAssembleShaderFromResourceA");
			p[4] = GetProcAddress(hL, "D3DXAssembleShaderFromResourceW");
			p[5] = GetProcAddress(hL, "D3DXBoxBoundProbe");
			p[6] = GetProcAddress(hL, "D3DXCheckCubeTextureRequirements");
			p[7] = GetProcAddress(hL, "D3DXCheckTextureRequirements");
			p[8] = GetProcAddress(hL, "D3DXCheckVersion");
			p[9] = GetProcAddress(hL, "D3DXCheckVolumeTextureRequirements");
			p[10] = GetProcAddress(hL, "D3DXCleanMesh");
			p[11] = GetProcAddress(hL, "D3DXColorAdjustContrast");
			p[12] = GetProcAddress(hL, "D3DXColorAdjustSaturation");
			p[13] = GetProcAddress(hL, "D3DXCompileShader");
			p[14] = GetProcAddress(hL, "D3DXCompileShaderFromFileA");
			p[15] = GetProcAddress(hL, "D3DXCompileShaderFromFileW");
			p[16] = GetProcAddress(hL, "D3DXCompileShaderFromResourceA");
			p[17] = GetProcAddress(hL, "D3DXCompileShaderFromResourceW");
			p[18] = GetProcAddress(hL, "D3DXComputeBoundingBox");
			p[19] = GetProcAddress(hL, "D3DXComputeBoundingSphere");
			p[20] = GetProcAddress(hL, "D3DXComputeIMTFromPerTexelSignal");
			p[21] = GetProcAddress(hL, "D3DXComputeIMTFromPerVertexSignal");
			p[22] = GetProcAddress(hL, "D3DXComputeIMTFromSignal");
			p[23] = GetProcAddress(hL, "D3DXComputeIMTFromTexture");
			p[24] = GetProcAddress(hL, "D3DXComputeNormalMap");
			p[25] = GetProcAddress(hL, "D3DXComputeNormals");
			p[26] = GetProcAddress(hL, "D3DXComputeTangent");
			p[27] = GetProcAddress(hL, "D3DXComputeTangentFrame");
			p[28] = GetProcAddress(hL, "D3DXComputeTangentFrameEx");
			p[29] = GetProcAddress(hL, "D3DXConcatenateMeshes");
			p[30] = GetProcAddress(hL, "D3DXConvertMeshSubsetToSingleStrip");
			p[31] = GetProcAddress(hL, "D3DXConvertMeshSubsetToStrips");
			p[32] = GetProcAddress(hL, "D3DXCreateAnimationController");
			p[33] = GetProcAddress(hL, "D3DXCreateBox");
			p[34] = GetProcAddress(hL, "D3DXCreateBuffer");
			p[35] = GetProcAddress(hL, "D3DXCreateCompressedAnimationSet");
			p[36] = GetProcAddress(hL, "D3DXCreateCubeTexture");
			p[37] = GetProcAddress(hL, "D3DXCreateCubeTextureFromFileA");
			p[38] = GetProcAddress(hL, "D3DXCreateCubeTextureFromFileExA");
			p[39] = GetProcAddress(hL, "D3DXCreateCubeTextureFromFileExW");
			p[40] = GetProcAddress(hL, "D3DXCreateCubeTextureFromFileInMemory");
			p[41] = GetProcAddress(hL, "D3DXCreateCubeTextureFromFileInMemoryEx");
			p[42] = GetProcAddress(hL, "D3DXCreateCubeTextureFromFileW");
			p[43] = GetProcAddress(hL, "D3DXCreateCubeTextureFromResourceA");
			p[44] = GetProcAddress(hL, "D3DXCreateCubeTextureFromResourceExA");
			p[45] = GetProcAddress(hL, "D3DXCreateCubeTextureFromResourceExW");
			p[46] = GetProcAddress(hL, "D3DXCreateCubeTextureFromResourceW");
			p[47] = GetProcAddress(hL, "D3DXCreateCylinder");
			p[48] = GetProcAddress(hL, "D3DXCreateEffect");
			p[49] = GetProcAddress(hL, "D3DXCreateEffectCompiler");
			p[50] = GetProcAddress(hL, "D3DXCreateEffectCompilerFromFileA");
			p[51] = GetProcAddress(hL, "D3DXCreateEffectCompilerFromFileW");
			p[52] = GetProcAddress(hL, "D3DXCreateEffectCompilerFromResourceA");
			p[53] = GetProcAddress(hL, "D3DXCreateEffectCompilerFromResourceW");
			p[54] = GetProcAddress(hL, "D3DXCreateEffectEx");
			p[55] = GetProcAddress(hL, "D3DXCreateEffectFromFileA");
			p[56] = GetProcAddress(hL, "D3DXCreateEffectFromFileExA");
			p[57] = GetProcAddress(hL, "D3DXCreateEffectFromFileExW");
			p[58] = GetProcAddress(hL, "D3DXCreateEffectFromFileW");
			p[59] = GetProcAddress(hL, "D3DXCreateEffectFromResourceA");
			p[60] = GetProcAddress(hL, "D3DXCreateEffectFromResourceExA");
			p[61] = GetProcAddress(hL, "D3DXCreateEffectFromResourceExW");
			p[62] = GetProcAddress(hL, "D3DXCreateEffectFromResourceW");
			p[63] = GetProcAddress(hL, "D3DXCreateEffectPool");
			p[64] = GetProcAddress(hL, "D3DXCreateFontA");
			p[65] = GetProcAddress(hL, "D3DXCreateFontIndirectA");
			p[66] = GetProcAddress(hL, "D3DXCreateFontIndirectW");
			p[67] = GetProcAddress(hL, "D3DXCreateFontW");
			p[68] = GetProcAddress(hL, "D3DXCreateKeyframedAnimationSet");
			p[69] = GetProcAddress(hL, "D3DXCreateLine");
			p[70] = GetProcAddress(hL, "D3DXCreateMatrixStack");
			p[71] = GetProcAddress(hL, "D3DXCreateMesh");
			p[72] = GetProcAddress(hL, "D3DXCreateMeshFVF");
			p[73] = GetProcAddress(hL, "D3DXCreateNPatchMesh");
			p[74] = GetProcAddress(hL, "D3DXCreatePMeshFromStream");
			p[75] = GetProcAddress(hL, "D3DXCreatePRTBuffer");
			p[76] = GetProcAddress(hL, "D3DXCreatePRTBufferTex");
			p[77] = GetProcAddress(hL, "D3DXCreatePRTCompBuffer");
			p[78] = GetProcAddress(hL, "D3DXCreatePRTEngine");
			p[79] = GetProcAddress(hL, "D3DXCreatePatchMesh");
			p[80] = GetProcAddress(hL, "D3DXCreatePolygon");
			p[81] = GetProcAddress(hL, "D3DXCreateRenderToEnvMap");
			p[82] = GetProcAddress(hL, "D3DXCreateRenderToSurface");
			p[83] = GetProcAddress(hL, "D3DXCreateSPMesh");
			p[84] = GetProcAddress(hL, "D3DXCreateSkinInfo");
			p[85] = GetProcAddress(hL, "D3DXCreateSkinInfoFVF");
			p[86] = GetProcAddress(hL, "D3DXCreateSkinInfoFromBlendedMesh");
			p[87] = GetProcAddress(hL, "D3DXCreateSphere");
			p[88] = GetProcAddress(hL, "D3DXCreateSprite");
			p[89] = GetProcAddress(hL, "D3DXCreateTeapot");
			p[90] = GetProcAddress(hL, "D3DXCreateTextA");
			p[91] = GetProcAddress(hL, "D3DXCreateTextW");
			p[92] = GetProcAddress(hL, "D3DXCreateTexture");
			p[93] = GetProcAddress(hL, "D3DXCreateTextureFromFileA");
			p[94] = GetProcAddress(hL, "D3DXCreateTextureFromFileExA");
			p[95] = GetProcAddress(hL, "D3DXCreateTextureFromFileExW");
			p[96] = GetProcAddress(hL, "D3DXCreateTextureFromFileInMemory");
			p[97] = GetProcAddress(hL, "D3DXCreateTextureFromFileInMemoryEx");
			p[98] = GetProcAddress(hL, "D3DXCreateTextureFromFileW");
			p[99] = GetProcAddress(hL, "D3DXCreateTextureFromResourceA");
			p[100] = GetProcAddress(hL, "D3DXCreateTextureFromResourceExA");
			p[101] = GetProcAddress(hL, "D3DXCreateTextureFromResourceExW");
			p[102] = GetProcAddress(hL, "D3DXCreateTextureFromResourceW");
			p[103] = GetProcAddress(hL, "D3DXCreateTextureGutterHelper");
			p[104] = GetProcAddress(hL, "D3DXCreateTextureShader");
			p[105] = GetProcAddress(hL, "D3DXCreateTorus");
			p[106] = GetProcAddress(hL, "D3DXCreateVolumeTexture");
			p[107] = GetProcAddress(hL, "D3DXCreateVolumeTextureFromFileA");
			p[108] = GetProcAddress(hL, "D3DXCreateVolumeTextureFromFileExA");
			p[109] = GetProcAddress(hL, "D3DXCreateVolumeTextureFromFileExW");
			p[110] = GetProcAddress(hL, "D3DXCreateVolumeTextureFromFileInMemory");
			p[111] = GetProcAddress(hL, "D3DXCreateVolumeTextureFromFileInMemoryEx");
			p[112] = GetProcAddress(hL, "D3DXCreateVolumeTextureFromFileW");
			p[113] = GetProcAddress(hL, "D3DXCreateVolumeTextureFromResourceA");
			p[114] = GetProcAddress(hL, "D3DXCreateVolumeTextureFromResourceExA");
			p[115] = GetProcAddress(hL, "D3DXCreateVolumeTextureFromResourceExW");
			p[116] = GetProcAddress(hL, "D3DXCreateVolumeTextureFromResourceW");
			p[117] = GetProcAddress(hL, "D3DXDebugMute");
			p[118] = GetProcAddress(hL, "D3DXDeclaratorFromFVF");
			p[119] = GetProcAddress(hL, "D3DXDisassembleEffect");
			p[120] = GetProcAddress(hL, "D3DXDisassembleShader");
			p[121] = GetProcAddress(hL, "D3DXFVFFromDeclarator");
			p[122] = GetProcAddress(hL, "D3DXFileCreate");
			p[123] = GetProcAddress(hL, "D3DXFillCubeTexture");
			p[124] = GetProcAddress(hL, "D3DXFillCubeTextureTX");
			p[125] = GetProcAddress(hL, "D3DXFillTexture");
			p[126] = GetProcAddress(hL, "D3DXFillTextureTX");
			p[127] = GetProcAddress(hL, "D3DXFillVolumeTexture");
			p[128] = GetProcAddress(hL, "D3DXFillVolumeTextureTX");
			p[129] = GetProcAddress(hL, "D3DXFilterTexture");
			p[130] = GetProcAddress(hL, "D3DXFindShaderComment");
			p[131] = GetProcAddress(hL, "D3DXFloat16To32Array");
			p[132] = GetProcAddress(hL, "D3DXFloat32To16Array");
			p[133] = GetProcAddress(hL, "D3DXFrameAppendChild");
			p[134] = GetProcAddress(hL, "D3DXFrameCalculateBoundingSphere");
			p[135] = GetProcAddress(hL, "D3DXFrameDestroy");
			p[136] = GetProcAddress(hL, "D3DXFrameFind");
			p[137] = GetProcAddress(hL, "D3DXFrameNumNamedMatrices");
			p[138] = GetProcAddress(hL, "D3DXFrameRegisterNamedMatrices");
			p[139] = GetProcAddress(hL, "D3DXFresnelTerm");
			p[140] = GetProcAddress(hL, "D3DXGenerateOutputDecl");
			p[141] = GetProcAddress(hL, "D3DXGeneratePMesh");
			p[142] = GetProcAddress(hL, "D3DXGetDeclLength");
			p[143] = GetProcAddress(hL, "D3DXGetDeclVertexSize");
			p[144] = GetProcAddress(hL, "D3DXGetDriverLevel");
			p[145] = GetProcAddress(hL, "D3DXGetFVFVertexSize");
			p[146] = GetProcAddress(hL, "D3DXGetImageInfoFromFileA");
			p[147] = GetProcAddress(hL, "D3DXGetImageInfoFromFileInMemory");
			p[148] = GetProcAddress(hL, "D3DXGetImageInfoFromFileW");
			p[149] = GetProcAddress(hL, "D3DXGetImageInfoFromResourceA");
			p[150] = GetProcAddress(hL, "D3DXGetImageInfoFromResourceW");
			p[151] = GetProcAddress(hL, "D3DXGetPixelShaderProfile");
			p[152] = GetProcAddress(hL, "D3DXGetShaderConstantTable");
			p[153] = GetProcAddress(hL, "D3DXGetShaderConstantTableEx");
			p[154] = GetProcAddress(hL, "D3DXGetShaderInputSemantics");
			p[155] = GetProcAddress(hL, "D3DXGetShaderOutputSemantics");
			p[156] = GetProcAddress(hL, "D3DXGetShaderSamplers");
			p[157] = GetProcAddress(hL, "D3DXGetShaderSize");
			p[158] = GetProcAddress(hL, "D3DXGetShaderVersion");
			p[159] = GetProcAddress(hL, "D3DXGetVertexShaderProfile");
			p[160] = GetProcAddress(hL, "D3DXIntersect");
			p[161] = GetProcAddress(hL, "D3DXIntersectSubset");
			p[162] = GetProcAddress(hL, "D3DXIntersectTri");
			p[163] = GetProcAddress(hL, "D3DXLoadMeshFromXA");
			p[164] = GetProcAddress(hL, "D3DXLoadMeshFromXInMemory");
			p[165] = GetProcAddress(hL, "D3DXLoadMeshFromXResource");
			p[166] = GetProcAddress(hL, "D3DXLoadMeshFromXW");
			p[167] = GetProcAddress(hL, "D3DXLoadMeshFromXof");
			p[168] = GetProcAddress(hL, "D3DXLoadMeshHierarchyFromXA");
			p[169] = GetProcAddress(hL, "D3DXLoadMeshHierarchyFromXInMemory");
			p[170] = GetProcAddress(hL, "D3DXLoadMeshHierarchyFromXW");
			p[171] = GetProcAddress(hL, "D3DXLoadPRTBufferFromFileA");
			p[172] = GetProcAddress(hL, "D3DXLoadPRTBufferFromFileW");
			p[173] = GetProcAddress(hL, "D3DXLoadPRTCompBufferFromFileA");
			p[174] = GetProcAddress(hL, "D3DXLoadPRTCompBufferFromFileW");
			p[175] = GetProcAddress(hL, "D3DXLoadPatchMeshFromXof");
			p[176] = GetProcAddress(hL, "D3DXLoadSkinMeshFromXof");
			p[177] = GetProcAddress(hL, "D3DXLoadSurfaceFromFileA");
			p[178] = GetProcAddress(hL, "D3DXLoadSurfaceFromFileInMemory");
			p[179] = GetProcAddress(hL, "D3DXLoadSurfaceFromFileW");
			p[180] = GetProcAddress(hL, "D3DXLoadSurfaceFromMemory");
			p[181] = GetProcAddress(hL, "D3DXLoadSurfaceFromResourceA");
			p[182] = GetProcAddress(hL, "D3DXLoadSurfaceFromResourceW");
			p[183] = GetProcAddress(hL, "D3DXLoadSurfaceFromSurface");
			p[184] = GetProcAddress(hL, "D3DXLoadVolumeFromFileA");
			p[185] = GetProcAddress(hL, "D3DXLoadVolumeFromFileInMemory");
			p[186] = GetProcAddress(hL, "D3DXLoadVolumeFromFileW");
			p[187] = GetProcAddress(hL, "D3DXLoadVolumeFromMemory");
			p[188] = GetProcAddress(hL, "D3DXLoadVolumeFromResourceA");
			p[189] = GetProcAddress(hL, "D3DXLoadVolumeFromResourceW");
			p[190] = GetProcAddress(hL, "D3DXLoadVolumeFromVolume");
			p[191] = GetProcAddress(hL, "D3DXMatrixAffineTransformation");
			p[192] = GetProcAddress(hL, "D3DXMatrixAffineTransformation2D");
			p[193] = GetProcAddress(hL, "D3DXMatrixDecompose");
			p[194] = GetProcAddress(hL, "D3DXMatrixDeterminant");
			p[195] = GetProcAddress(hL, "D3DXMatrixInverse");
			p[196] = GetProcAddress(hL, "D3DXMatrixLookAtLH");
			p[197] = GetProcAddress(hL, "D3DXMatrixLookAtRH");
			p[198] = GetProcAddress(hL, "D3DXMatrixMultiply");
			p[199] = GetProcAddress(hL, "D3DXMatrixMultiplyTranspose");
			p[200] = GetProcAddress(hL, "D3DXMatrixOrthoLH");
			p[201] = GetProcAddress(hL, "D3DXMatrixOrthoOffCenterLH");
			p[202] = GetProcAddress(hL, "D3DXMatrixOrthoOffCenterRH");
			p[203] = GetProcAddress(hL, "D3DXMatrixOrthoRH");
			p[204] = GetProcAddress(hL, "D3DXMatrixPerspectiveFovLH");
			p[205] = GetProcAddress(hL, "D3DXMatrixPerspectiveFovRH");
			p[206] = GetProcAddress(hL, "D3DXMatrixPerspectiveLH");
			p[207] = GetProcAddress(hL, "D3DXMatrixPerspectiveOffCenterLH");
			p[208] = GetProcAddress(hL, "D3DXMatrixPerspectiveOffCenterRH");
			p[209] = GetProcAddress(hL, "D3DXMatrixPerspectiveRH");
			p[210] = GetProcAddress(hL, "D3DXMatrixReflect");
			p[211] = GetProcAddress(hL, "D3DXMatrixRotationAxis");
			p[212] = GetProcAddress(hL, "D3DXMatrixRotationQuaternion");
			p[213] = GetProcAddress(hL, "D3DXMatrixRotationX");
			p[214] = GetProcAddress(hL, "D3DXMatrixRotationY");
			p[215] = GetProcAddress(hL, "D3DXMatrixRotationYawPitchRoll");
			p[216] = GetProcAddress(hL, "D3DXMatrixRotationZ");
			p[217] = GetProcAddress(hL, "D3DXMatrixScaling");
			p[218] = GetProcAddress(hL, "D3DXMatrixShadow");
			p[219] = GetProcAddress(hL, "D3DXMatrixTransformation");
			p[220] = GetProcAddress(hL, "D3DXMatrixTransformation2D");
			p[221] = GetProcAddress(hL, "D3DXMatrixTranslation");
			p[222] = GetProcAddress(hL, "D3DXMatrixTranspose");
			p[223] = GetProcAddress(hL, "D3DXOptimizeFaces");
			p[224] = GetProcAddress(hL, "D3DXOptimizeVertices");
			p[225] = GetProcAddress(hL, "D3DXPlaneFromPointNormal");
			p[226] = GetProcAddress(hL, "D3DXPlaneFromPoints");
			p[227] = GetProcAddress(hL, "D3DXPlaneIntersectLine");
			p[228] = GetProcAddress(hL, "D3DXPlaneNormalize");
			p[229] = GetProcAddress(hL, "D3DXPlaneTransform");
			p[230] = GetProcAddress(hL, "D3DXPlaneTransformArray");
			p[231] = GetProcAddress(hL, "D3DXPreprocessShader");
			p[232] = GetProcAddress(hL, "D3DXPreprocessShaderFromFileA");
			p[233] = GetProcAddress(hL, "D3DXPreprocessShaderFromFileW");
			p[234] = GetProcAddress(hL, "D3DXPreprocessShaderFromResourceA");
			p[235] = GetProcAddress(hL, "D3DXPreprocessShaderFromResourceW");
			p[236] = GetProcAddress(hL, "D3DXQuaternionBaryCentric");
			p[237] = GetProcAddress(hL, "D3DXQuaternionExp");
			p[238] = GetProcAddress(hL, "D3DXQuaternionInverse");
			p[239] = GetProcAddress(hL, "D3DXQuaternionLn");
			p[240] = GetProcAddress(hL, "D3DXQuaternionMultiply");
			p[241] = GetProcAddress(hL, "D3DXQuaternionNormalize");
			p[242] = GetProcAddress(hL, "D3DXQuaternionRotationAxis");
			p[243] = GetProcAddress(hL, "D3DXQuaternionRotationMatrix");
			p[244] = GetProcAddress(hL, "D3DXQuaternionRotationYawPitchRoll");
			p[245] = GetProcAddress(hL, "D3DXQuaternionSlerp");
			p[246] = GetProcAddress(hL, "D3DXQuaternionSquad");
			p[247] = GetProcAddress(hL, "D3DXQuaternionSquadSetup");
			p[248] = GetProcAddress(hL, "D3DXQuaternionToAxisAngle");
			p[249] = GetProcAddress(hL, "D3DXRectPatchSize");
			p[250] = GetProcAddress(hL, "D3DXSHAdd");
			p[251] = GetProcAddress(hL, "D3DXSHDot");
			p[252] = GetProcAddress(hL, "D3DXSHEvalConeLight");
			p[253] = GetProcAddress(hL, "D3DXSHEvalDirection");
			p[254] = GetProcAddress(hL, "D3DXSHEvalDirectionalLight");
			p[255] = GetProcAddress(hL, "D3DXSHEvalHemisphereLight");
			p[256] = GetProcAddress(hL, "D3DXSHEvalSphericalLight");
			p[257] = GetProcAddress(hL, "D3DXSHMultiply2");
			p[258] = GetProcAddress(hL, "D3DXSHMultiply3");
			p[259] = GetProcAddress(hL, "D3DXSHMultiply4");
			p[260] = GetProcAddress(hL, "D3DXSHMultiply5");
			p[261] = GetProcAddress(hL, "D3DXSHMultiply6");
			p[262] = GetProcAddress(hL, "D3DXSHPRTCompSplitMeshSC");
			p[263] = GetProcAddress(hL, "D3DXSHPRTCompSuperCluster");
			p[264] = GetProcAddress(hL, "D3DXSHProjectCubeMap");
			p[265] = GetProcAddress(hL, "D3DXSHRotate");
			p[266] = GetProcAddress(hL, "D3DXSHRotateZ");
			p[267] = GetProcAddress(hL, "D3DXSHScale");
			p[268] = GetProcAddress(hL, "D3DXSaveMeshHierarchyToFileA");
			p[269] = GetProcAddress(hL, "D3DXSaveMeshHierarchyToFileW");
			p[270] = GetProcAddress(hL, "D3DXSaveMeshToXA");
			p[271] = GetProcAddress(hL, "D3DXSaveMeshToXW");
			p[272] = GetProcAddress(hL, "D3DXSavePRTBufferToFileA");
			p[273] = GetProcAddress(hL, "D3DXSavePRTBufferToFileW");
			p[274] = GetProcAddress(hL, "D3DXSavePRTCompBufferToFileA");
			p[275] = GetProcAddress(hL, "D3DXSavePRTCompBufferToFileW");
			p[276] = GetProcAddress(hL, "D3DXSaveSurfaceToFileA");
			p[277] = GetProcAddress(hL, "D3DXSaveSurfaceToFileInMemory");
			p[278] = GetProcAddress(hL, "D3DXSaveSurfaceToFileW");
			p[279] = GetProcAddress(hL, "D3DXSaveTextureToFileA");
			p[280] = GetProcAddress(hL, "D3DXSaveTextureToFileInMemory");
			p[281] = GetProcAddress(hL, "D3DXSaveTextureToFileW");
			p[282] = GetProcAddress(hL, "D3DXSaveVolumeToFileA");
			p[283] = GetProcAddress(hL, "D3DXSaveVolumeToFileInMemory");
			p[284] = GetProcAddress(hL, "D3DXSaveVolumeToFileW");
			p[285] = GetProcAddress(hL, "D3DXSimplifyMesh");
			p[286] = GetProcAddress(hL, "D3DXSphereBoundProbe");
			p[287] = GetProcAddress(hL, "D3DXSplitMesh");
			p[288] = GetProcAddress(hL, "D3DXTessellateNPatches");
			p[289] = GetProcAddress(hL, "D3DXTessellateRectPatch");
			p[290] = GetProcAddress(hL, "D3DXTessellateTriPatch");
			p[291] = GetProcAddress(hL, "D3DXTriPatchSize");
			p[292] = GetProcAddress(hL, "D3DXUVAtlasCreate");
			p[293] = GetProcAddress(hL, "D3DXUVAtlasPack");
			p[294] = GetProcAddress(hL, "D3DXUVAtlasPartition");
			p[295] = GetProcAddress(hL, "D3DXValidMesh");
			p[296] = GetProcAddress(hL, "D3DXValidPatchMesh");
			p[297] = GetProcAddress(hL, "D3DXVec2BaryCentric");
			p[298] = GetProcAddress(hL, "D3DXVec2CatmullRom");
			p[299] = GetProcAddress(hL, "D3DXVec2Hermite");
			p[300] = GetProcAddress(hL, "D3DXVec2Normalize");
			p[301] = GetProcAddress(hL, "D3DXVec2Transform");
			p[302] = GetProcAddress(hL, "D3DXVec2TransformArray");
			p[303] = GetProcAddress(hL, "D3DXVec2TransformCoord");
			p[304] = GetProcAddress(hL, "D3DXVec2TransformCoordArray");
			p[305] = GetProcAddress(hL, "D3DXVec2TransformNormal");
			p[306] = GetProcAddress(hL, "D3DXVec2TransformNormalArray");
			p[307] = GetProcAddress(hL, "D3DXVec3BaryCentric");
			p[308] = GetProcAddress(hL, "D3DXVec3CatmullRom");
			p[309] = GetProcAddress(hL, "D3DXVec3Hermite");
			p[310] = GetProcAddress(hL, "D3DXVec3Normalize");
			p[311] = GetProcAddress(hL, "D3DXVec3Project");
			p[312] = GetProcAddress(hL, "D3DXVec3ProjectArray");
			p[313] = GetProcAddress(hL, "D3DXVec3Transform");
			p[314] = GetProcAddress(hL, "D3DXVec3TransformArray");
			p[315] = GetProcAddress(hL, "D3DXVec3TransformCoord");
			p[316] = GetProcAddress(hL, "D3DXVec3TransformCoordArray");
			p[317] = GetProcAddress(hL, "D3DXVec3TransformNormal");
			p[318] = GetProcAddress(hL, "D3DXVec3TransformNormalArray");
			p[319] = GetProcAddress(hL, "D3DXVec3Unproject");
			p[320] = GetProcAddress(hL, "D3DXVec3UnprojectArray");
			p[321] = GetProcAddress(hL, "D3DXVec4BaryCentric");
			p[322] = GetProcAddress(hL, "D3DXVec4CatmullRom");
			p[323] = GetProcAddress(hL, "D3DXVec4Cross");
			p[324] = GetProcAddress(hL, "D3DXVec4Hermite");
			p[325] = GetProcAddress(hL, "D3DXVec4Normalize");
			p[326] = GetProcAddress(hL, "D3DXVec4Transform");
			p[327] = GetProcAddress(hL, "D3DXVec4TransformArray");
			p[328] = GetProcAddress(hL, "D3DXWeldVertices");

			logFile << "success" << std::endl;

			if (_tcscmp(exeName, _T("CreationKit.exe")) == 0)
			{
				logFile << "creation kit detected, injecting CK patch dlls" << std::endl;
				loadedPlugins = true;
				LoadCKPlugins();
			}
		}
	}

	if (reason == DLL_PROCESS_DETACH)
	{
		if (loadedPlugins)
		{
			if (!loadedLib.empty())
			{
				for (auto lib : loadedLib)
					FreeLibrary(lib);
				loadedLib.clear();
			}
		}

		if (hL)
		{
			FreeLibrary(hL);
		}
		return 1;
	}

	return 1;
}

extern "C"
{
	FARPROC PA = NULL;
	int RunASM();

	void PROXY_D3DXAssembleShader() {
		PA = p[0];
		RunASM();
	}
	void PROXY_D3DXAssembleShaderFromFileA() {
		PA = p[1];
		RunASM();
	}
	void PROXY_D3DXAssembleShaderFromFileW() {
		PA = p[2];
		RunASM();
	}
	void PROXY_D3DXAssembleShaderFromResourceA() {
		PA = p[3];
		RunASM();
	}
	void PROXY_D3DXAssembleShaderFromResourceW() {
		PA = p[4];
		RunASM();
	}
	void PROXY_D3DXBoxBoundProbe() {
		PA = p[5];
		RunASM();
	}
	void PROXY_D3DXCheckCubeTextureRequirements() {
		PA = p[6];
		RunASM();
	}
	void PROXY_D3DXCheckTextureRequirements() {
		PA = p[7];
		RunASM();
	}
	void PROXY_D3DXCheckVersion() {
		PA = p[8];
		RunASM();
	}
	void PROXY_D3DXCheckVolumeTextureRequirements() {
		PA = p[9];
		RunASM();
	}
	void PROXY_D3DXCleanMesh() {
		PA = p[10];
		RunASM();
	}
	void PROXY_D3DXColorAdjustContrast() {
		PA = p[11];
		RunASM();
	}
	void PROXY_D3DXColorAdjustSaturation() {
		PA = p[12];
		RunASM();
	}
	void PROXY_D3DXCompileShader() {
		PA = p[13];
		RunASM();
	}
	void PROXY_D3DXCompileShaderFromFileA() {
		PA = p[14];
		RunASM();
	}
	void PROXY_D3DXCompileShaderFromFileW() {
		PA = p[15];
		RunASM();
	}
	void PROXY_D3DXCompileShaderFromResourceA() {
		PA = p[16];
		RunASM();
	}
	void PROXY_D3DXCompileShaderFromResourceW() {
		PA = p[17];
		RunASM();
	}
	void PROXY_D3DXComputeBoundingBox() {
		PA = p[18];
		RunASM();
	}
	void PROXY_D3DXComputeBoundingSphere() {
		PA = p[19];
		RunASM();
	}
	void PROXY_D3DXComputeIMTFromPerTexelSignal() {
		PA = p[20];
		RunASM();
	}
	void PROXY_D3DXComputeIMTFromPerVertexSignal() {
		PA = p[21];
		RunASM();
	}
	void PROXY_D3DXComputeIMTFromSignal() {
		PA = p[22];
		RunASM();
	}
	void PROXY_D3DXComputeIMTFromTexture() {
		PA = p[23];
		RunASM();
	}
	void PROXY_D3DXComputeNormalMap() {
		PA = p[24];
		RunASM();
	}
	void PROXY_D3DXComputeNormals() {
		PA = p[25];
		RunASM();
	}
	void PROXY_D3DXComputeTangent() {
		PA = p[26];
		RunASM();
	}
	void PROXY_D3DXComputeTangentFrame() {
		PA = p[27];
		RunASM();
	}
	void PROXY_D3DXComputeTangentFrameEx() {
		PA = p[28];
		RunASM();
	}
	void PROXY_D3DXConcatenateMeshes() {
		PA = p[29];
		RunASM();
	}
	void PROXY_D3DXConvertMeshSubsetToSingleStrip() {
		PA = p[30];
		RunASM();
	}
	void PROXY_D3DXConvertMeshSubsetToStrips() {
		PA = p[31];
		RunASM();
	}
	void PROXY_D3DXCreateAnimationController() {
		PA = p[32];
		RunASM();
	}
	void PROXY_D3DXCreateBox() {
		PA = p[33];
		RunASM();
	}
	void PROXY_D3DXCreateBuffer() {
		PA = p[34];
		RunASM();
	}
	void PROXY_D3DXCreateCompressedAnimationSet() {
		PA = p[35];
		RunASM();
	}
	void PROXY_D3DXCreateCubeTexture() {
		PA = p[36];
		RunASM();
	}
	void PROXY_D3DXCreateCubeTextureFromFileA() {
		PA = p[37];
		RunASM();
	}
	void PROXY_D3DXCreateCubeTextureFromFileExA() {
		PA = p[38];
		RunASM();
	}
	void PROXY_D3DXCreateCubeTextureFromFileExW() {
		PA = p[39];
		RunASM();
	}
	void PROXY_D3DXCreateCubeTextureFromFileInMemory() {
		PA = p[40];
		RunASM();
	}
	void PROXY_D3DXCreateCubeTextureFromFileInMemoryEx() {
		PA = p[41];
		RunASM();
	}
	void PROXY_D3DXCreateCubeTextureFromFileW() {
		PA = p[42];
		RunASM();
	}
	void PROXY_D3DXCreateCubeTextureFromResourceA() {
		PA = p[43];
		RunASM();
	}
	void PROXY_D3DXCreateCubeTextureFromResourceExA() {
		PA = p[44];
		RunASM();
	}
	void PROXY_D3DXCreateCubeTextureFromResourceExW() {
		PA = p[45];
		RunASM();
	}
	void PROXY_D3DXCreateCubeTextureFromResourceW() {
		PA = p[46];
		RunASM();
	}
	void PROXY_D3DXCreateCylinder() {
		PA = p[47];
		RunASM();
	}
	void PROXY_D3DXCreateEffect() {
		PA = p[48];
		RunASM();
	}
	void PROXY_D3DXCreateEffectCompiler() {
		PA = p[49];
		RunASM();
	}
	void PROXY_D3DXCreateEffectCompilerFromFileA() {
		PA = p[50];
		RunASM();
	}
	void PROXY_D3DXCreateEffectCompilerFromFileW() {
		PA = p[51];
		RunASM();
	}
	void PROXY_D3DXCreateEffectCompilerFromResourceA() {
		PA = p[52];
		RunASM();
	}
	void PROXY_D3DXCreateEffectCompilerFromResourceW() {
		PA = p[53];
		RunASM();
	}
	void PROXY_D3DXCreateEffectEx() {
		PA = p[54];
		RunASM();
	}
	void PROXY_D3DXCreateEffectFromFileA() {
		PA = p[55];
		RunASM();
	}
	void PROXY_D3DXCreateEffectFromFileExA() {
		PA = p[56];
		RunASM();
	}
	void PROXY_D3DXCreateEffectFromFileExW() {
		PA = p[57];
		RunASM();
	}
	void PROXY_D3DXCreateEffectFromFileW() {
		PA = p[58];
		RunASM();
	}
	void PROXY_D3DXCreateEffectFromResourceA() {
		PA = p[59];
		RunASM();
	}
	void PROXY_D3DXCreateEffectFromResourceExA() {
		PA = p[60];
		RunASM();
	}
	void PROXY_D3DXCreateEffectFromResourceExW() {
		PA = p[61];
		RunASM();
	}
	void PROXY_D3DXCreateEffectFromResourceW() {
		PA = p[62];
		RunASM();
	}
	void PROXY_D3DXCreateEffectPool() {
		PA = p[63];
		RunASM();
	}
	void PROXY_D3DXCreateFontA() {
		PA = p[64];
		RunASM();
	}
	void PROXY_D3DXCreateFontIndirectA() {
		PA = p[65];
		RunASM();
	}
	void PROXY_D3DXCreateFontIndirectW() {
		PA = p[66];
		RunASM();
	}
	void PROXY_D3DXCreateFontW() {
		PA = p[67];
		RunASM();
	}
	void PROXY_D3DXCreateKeyframedAnimationSet() {
		PA = p[68];
		RunASM();
	}
	void PROXY_D3DXCreateLine() {
		PA = p[69];
		RunASM();
	}
	void PROXY_D3DXCreateMatrixStack() {
		PA = p[70];
		RunASM();
	}
	void PROXY_D3DXCreateMesh() {
		PA = p[71];
		RunASM();
	}
	void PROXY_D3DXCreateMeshFVF() {
		PA = p[72];
		RunASM();
	}
	void PROXY_D3DXCreateNPatchMesh() {
		PA = p[73];
		RunASM();
	}
	void PROXY_D3DXCreatePMeshFromStream() {
		PA = p[74];
		RunASM();
	}
	void PROXY_D3DXCreatePRTBuffer() {
		PA = p[75];
		RunASM();
	}
	void PROXY_D3DXCreatePRTBufferTex() {
		PA = p[76];
		RunASM();
	}
	void PROXY_D3DXCreatePRTCompBuffer() {
		PA = p[77];
		RunASM();
	}
	void PROXY_D3DXCreatePRTEngine() {
		PA = p[78];
		RunASM();
	}
	void PROXY_D3DXCreatePatchMesh() {
		PA = p[79];
		RunASM();
	}
	void PROXY_D3DXCreatePolygon() {
		PA = p[80];
		RunASM();
	}
	void PROXY_D3DXCreateRenderToEnvMap() {
		PA = p[81];
		RunASM();
	}
	void PROXY_D3DXCreateRenderToSurface() {
		PA = p[82];
		RunASM();
	}
	void PROXY_D3DXCreateSPMesh() {
		PA = p[83];
		RunASM();
	}
	void PROXY_D3DXCreateSkinInfo() {
		PA = p[84];
		RunASM();
	}
	void PROXY_D3DXCreateSkinInfoFVF() {
		PA = p[85];
		RunASM();
	}
	void PROXY_D3DXCreateSkinInfoFromBlendedMesh() {
		PA = p[86];
		RunASM();
	}
	void PROXY_D3DXCreateSphere() {
		PA = p[87];
		RunASM();
	}
	void PROXY_D3DXCreateSprite() {
		PA = p[88];
		RunASM();
	}
	void PROXY_D3DXCreateTeapot() {
		PA = p[89];
		RunASM();
	}
	void PROXY_D3DXCreateTextA() {
		PA = p[90];
		RunASM();
	}
	void PROXY_D3DXCreateTextW() {
		PA = p[91];
		RunASM();
	}
	void PROXY_D3DXCreateTexture() {
		PA = p[92];
		RunASM();
	}
	void PROXY_D3DXCreateTextureFromFileA() {
		PA = p[93];
		RunASM();
	}
	void PROXY_D3DXCreateTextureFromFileExA() {
		PA = p[94];
		RunASM();
	}
	void PROXY_D3DXCreateTextureFromFileExW() {
		PA = p[95];
		RunASM();
	}
	void PROXY_D3DXCreateTextureFromFileInMemory() {
		PA = p[96];
		RunASM();
	}
	void PROXY_D3DXCreateTextureFromFileInMemoryEx() {
		PA = p[97];
		RunASM();
	}
	void PROXY_D3DXCreateTextureFromFileW() {
		PA = p[98];
		RunASM();
	}
	void PROXY_D3DXCreateTextureFromResourceA() {
		PA = p[99];
		RunASM();
	}
	void PROXY_D3DXCreateTextureFromResourceExA() {
		PA = p[100];
		RunASM();
	}
	void PROXY_D3DXCreateTextureFromResourceExW() {
		PA = p[101];
		RunASM();
	}
	void PROXY_D3DXCreateTextureFromResourceW() {
		PA = p[102];
		RunASM();
	}
	void PROXY_D3DXCreateTextureGutterHelper() {
		PA = p[103];
		RunASM();
	}
	void PROXY_D3DXCreateTextureShader() {
		PA = p[104];
		RunASM();
	}
	void PROXY_D3DXCreateTorus() {
		PA = p[105];
		RunASM();
	}
	void PROXY_D3DXCreateVolumeTexture() {
		PA = p[106];
		RunASM();
	}
	void PROXY_D3DXCreateVolumeTextureFromFileA() {
		PA = p[107];
		RunASM();
	}
	void PROXY_D3DXCreateVolumeTextureFromFileExA() {
		PA = p[108];
		RunASM();
	}
	void PROXY_D3DXCreateVolumeTextureFromFileExW() {
		PA = p[109];
		RunASM();
	}
	void PROXY_D3DXCreateVolumeTextureFromFileInMemory() {
		PA = p[110];
		RunASM();
	}
	void PROXY_D3DXCreateVolumeTextureFromFileInMemoryEx() {
		PA = p[111];
		RunASM();
	}
	void PROXY_D3DXCreateVolumeTextureFromFileW() {
		PA = p[112];
		RunASM();
	}
	void PROXY_D3DXCreateVolumeTextureFromResourceA() {
		PA = p[113];
		RunASM();
	}
	void PROXY_D3DXCreateVolumeTextureFromResourceExA() {
		PA = p[114];
		RunASM();
	}
	void PROXY_D3DXCreateVolumeTextureFromResourceExW() {
		PA = p[115];
		RunASM();
	}
	void PROXY_D3DXCreateVolumeTextureFromResourceW() {
		PA = p[116];
		RunASM();
	}
	void PROXY_D3DXDebugMute() {
		PA = p[117];
		RunASM();
	}
	void PROXY_D3DXDeclaratorFromFVF() {
		PA = p[118];
		RunASM();
	}
	void PROXY_D3DXDisassembleEffect() {
		PA = p[119];
		RunASM();
	}
	void PROXY_D3DXDisassembleShader() {
		PA = p[120];
		RunASM();
	}
	void PROXY_D3DXFVFFromDeclarator() {
		PA = p[121];
		RunASM();
	}
	void PROXY_D3DXFileCreate() {
		PA = p[122];
		RunASM();
	}
	void PROXY_D3DXFillCubeTexture() {
		PA = p[123];
		RunASM();
	}
	void PROXY_D3DXFillCubeTextureTX() {
		PA = p[124];
		RunASM();
	}
	void PROXY_D3DXFillTexture() {
		PA = p[125];
		RunASM();
	}
	void PROXY_D3DXFillTextureTX() {
		PA = p[126];
		RunASM();
	}
	void PROXY_D3DXFillVolumeTexture() {
		PA = p[127];
		RunASM();
	}
	void PROXY_D3DXFillVolumeTextureTX() {
		PA = p[128];
		RunASM();
	}
	void PROXY_D3DXFilterTexture() {
		PA = p[129];
		RunASM();
	}
	void PROXY_D3DXFindShaderComment() {
		PA = p[130];
		RunASM();
	}
	void PROXY_D3DXFloat16To32Array() {
		PA = p[131];
		RunASM();
	}
	void PROXY_D3DXFloat32To16Array() {
		PA = p[132];
		RunASM();
	}
	void PROXY_D3DXFrameAppendChild() {
		PA = p[133];
		RunASM();
	}
	void PROXY_D3DXFrameCalculateBoundingSphere() {
		PA = p[134];
		RunASM();
	}
	void PROXY_D3DXFrameDestroy() {
		PA = p[135];
		RunASM();
	}
	void PROXY_D3DXFrameFind() {
		PA = p[136];
		RunASM();
	}
	void PROXY_D3DXFrameNumNamedMatrices() {
		PA = p[137];
		RunASM();
	}
	void PROXY_D3DXFrameRegisterNamedMatrices() {
		PA = p[138];
		RunASM();
	}
	void PROXY_D3DXFresnelTerm() {
		PA = p[139];
		RunASM();
	}
	void PROXY_D3DXGenerateOutputDecl() {
		PA = p[140];
		RunASM();
	}
	void PROXY_D3DXGeneratePMesh() {
		PA = p[141];
		RunASM();
	}
	void PROXY_D3DXGetDeclLength() {
		PA = p[142];
		RunASM();
	}
	void PROXY_D3DXGetDeclVertexSize() {
		PA = p[143];
		RunASM();
	}
	void PROXY_D3DXGetDriverLevel() {
		PA = p[144];
		RunASM();
	}
	void PROXY_D3DXGetFVFVertexSize() {
		PA = p[145];
		RunASM();
	}
	void PROXY_D3DXGetImageInfoFromFileA() {
		PA = p[146];
		RunASM();
	}
	void PROXY_D3DXGetImageInfoFromFileInMemory() {
		PA = p[147];
		RunASM();
	}
	void PROXY_D3DXGetImageInfoFromFileW() {
		PA = p[148];
		RunASM();
	}
	void PROXY_D3DXGetImageInfoFromResourceA() {
		PA = p[149];
		RunASM();
	}
	void PROXY_D3DXGetImageInfoFromResourceW() {
		PA = p[150];
		RunASM();
	}
	void PROXY_D3DXGetPixelShaderProfile() {
		PA = p[151];
		RunASM();
	}
	void PROXY_D3DXGetShaderConstantTable() {
		PA = p[152];
		RunASM();
	}
	void PROXY_D3DXGetShaderConstantTableEx() {
		PA = p[153];
		RunASM();
	}
	void PROXY_D3DXGetShaderInputSemantics() {
		PA = p[154];
		RunASM();
	}
	void PROXY_D3DXGetShaderOutputSemantics() {
		PA = p[155];
		RunASM();
	}
	void PROXY_D3DXGetShaderSamplers() {
		PA = p[156];
		RunASM();
	}
	void PROXY_D3DXGetShaderSize() {
		PA = p[157];
		RunASM();
	}
	void PROXY_D3DXGetShaderVersion() {
		PA = p[158];
		RunASM();
	}
	void PROXY_D3DXGetVertexShaderProfile() {
		PA = p[159];
		RunASM();
	}
	void PROXY_D3DXIntersect() {
		PA = p[160];
		RunASM();
	}
	void PROXY_D3DXIntersectSubset() {
		PA = p[161];
		RunASM();
	}
	void PROXY_D3DXIntersectTri() {
		PA = p[162];
		RunASM();
	}
	void PROXY_D3DXLoadMeshFromXA() {
		PA = p[163];
		RunASM();
	}
	void PROXY_D3DXLoadMeshFromXInMemory() {
		PA = p[164];
		RunASM();
	}
	void PROXY_D3DXLoadMeshFromXResource() {
		PA = p[165];
		RunASM();
	}
	void PROXY_D3DXLoadMeshFromXW() {
		PA = p[166];
		RunASM();
	}
	void PROXY_D3DXLoadMeshFromXof() {
		PA = p[167];
		RunASM();
	}
	void PROXY_D3DXLoadMeshHierarchyFromXA() {
		PA = p[168];
		RunASM();
	}
	void PROXY_D3DXLoadMeshHierarchyFromXInMemory() {
		PA = p[169];
		RunASM();
	}
	void PROXY_D3DXLoadMeshHierarchyFromXW() {
		PA = p[170];
		RunASM();
	}
	void PROXY_D3DXLoadPRTBufferFromFileA() {
		PA = p[171];
		RunASM();
	}
	void PROXY_D3DXLoadPRTBufferFromFileW() {
		PA = p[172];
		RunASM();
	}
	void PROXY_D3DXLoadPRTCompBufferFromFileA() {
		PA = p[173];
		RunASM();
	}
	void PROXY_D3DXLoadPRTCompBufferFromFileW() {
		PA = p[174];
		RunASM();
	}
	void PROXY_D3DXLoadPatchMeshFromXof() {
		PA = p[175];
		RunASM();
	}
	void PROXY_D3DXLoadSkinMeshFromXof() {
		PA = p[176];
		RunASM();
	}
	void PROXY_D3DXLoadSurfaceFromFileA() {
		PA = p[177];
		RunASM();
	}
	void PROXY_D3DXLoadSurfaceFromFileInMemory() {
		PA = p[178];
		RunASM();
	}
	void PROXY_D3DXLoadSurfaceFromFileW() {
		PA = p[179];
		RunASM();
	}
	void PROXY_D3DXLoadSurfaceFromMemory() {
		PA = p[180];
		RunASM();
	}
	void PROXY_D3DXLoadSurfaceFromResourceA() {
		PA = p[181];
		RunASM();
	}
	void PROXY_D3DXLoadSurfaceFromResourceW() {
		PA = p[182];
		RunASM();
	}
	void PROXY_D3DXLoadSurfaceFromSurface() {
		PA = p[183];
		RunASM();
	}
	void PROXY_D3DXLoadVolumeFromFileA() {
		PA = p[184];
		RunASM();
	}
	void PROXY_D3DXLoadVolumeFromFileInMemory() {
		PA = p[185];
		RunASM();
	}
	void PROXY_D3DXLoadVolumeFromFileW() {
		PA = p[186];
		RunASM();
	}
	void PROXY_D3DXLoadVolumeFromMemory() {
		PA = p[187];
		RunASM();
	}
	void PROXY_D3DXLoadVolumeFromResourceA() {
		PA = p[188];
		RunASM();
	}
	void PROXY_D3DXLoadVolumeFromResourceW() {
		PA = p[189];
		RunASM();
	}
	void PROXY_D3DXLoadVolumeFromVolume() {
		PA = p[190];
		RunASM();
	}
	void PROXY_D3DXMatrixAffineTransformation() {
		PA = p[191];
		RunASM();
	}
	void PROXY_D3DXMatrixAffineTransformation2D() {
		PA = p[192];
		RunASM();
	}
	void PROXY_D3DXMatrixDecompose() {
		PA = p[193];
		RunASM();
	}
	void PROXY_D3DXMatrixDeterminant() {
		PA = p[194];
		RunASM();
	}
	/*void PROXY_D3DXMatrixInverse() {
		PA = p[195];
		RunASM();
	}*/
	void PROXY_D3DXMatrixLookAtLH() {
		PA = p[196];
		RunASM();
	}
	void PROXY_D3DXMatrixLookAtRH() {
		PA = p[197];
		RunASM();
	}
	/*
	void PROXY_D3DXMatrixMultiply() {
		PA = p[198];
		RunASM();
	}
	void PROXY_D3DXMatrixMultiplyTranspose() {
		PA = p[199];
		RunASM();
	}*/
	void PROXY_D3DXMatrixOrthoLH() {
		PA = p[200];
		RunASM();
	}
	void PROXY_D3DXMatrixOrthoOffCenterLH() {
		PA = p[201];
		RunASM();
	}
	void PROXY_D3DXMatrixOrthoOffCenterRH() {
		PA = p[202];
		RunASM();
	}
	void PROXY_D3DXMatrixOrthoRH() {
		PA = p[203];
		RunASM();
	}
	void PROXY_D3DXMatrixPerspectiveFovLH() {
		PA = p[204];
		RunASM();
	}
	void PROXY_D3DXMatrixPerspectiveFovRH() {
		PA = p[205];
		RunASM();
	}
	void PROXY_D3DXMatrixPerspectiveLH() {
		PA = p[206];
		RunASM();
	}
	void PROXY_D3DXMatrixPerspectiveOffCenterLH() {
		PA = p[207];
		RunASM();
	}
	void PROXY_D3DXMatrixPerspectiveOffCenterRH() {
		PA = p[208];
		RunASM();
	}
	void PROXY_D3DXMatrixPerspectiveRH() {
		PA = p[209];
		RunASM();
	}
	void PROXY_D3DXMatrixReflect() {
		PA = p[210];
		RunASM();
	}
	void PROXY_D3DXMatrixRotationAxis() {
		PA = p[211];
		RunASM();
	}
	void PROXY_D3DXMatrixRotationQuaternion() {
		PA = p[212];
		RunASM();
	}
	void PROXY_D3DXMatrixRotationX() {
		PA = p[213];
		RunASM();
	}
	void PROXY_D3DXMatrixRotationY() {
		PA = p[214];
		RunASM();
	}
	void PROXY_D3DXMatrixRotationYawPitchRoll() {
		PA = p[215];
		RunASM();
	}
	void PROXY_D3DXMatrixRotationZ() {
		PA = p[216];
		RunASM();
	}
	void PROXY_D3DXMatrixScaling() {
		PA = p[217];
		RunASM();
	}
	void PROXY_D3DXMatrixShadow() {
		PA = p[218];
		RunASM();
	}
	void PROXY_D3DXMatrixTransformation() {
		PA = p[219];
		RunASM();
	}
	void PROXY_D3DXMatrixTransformation2D() {
		PA = p[220];
		RunASM();
	}
	void PROXY_D3DXMatrixTranslation() {
		PA = p[221];
		RunASM();
	}
	/*
	void PROXY_D3DXMatrixTranspose() {
		PA = p[222];
		RunASM();
	}*/
	void PROXY_D3DXOptimizeFaces() {
		PA = p[223];
		RunASM();
	}
	void PROXY_D3DXOptimizeVertices() {
		PA = p[224];
		RunASM();
	}
	void PROXY_D3DXPlaneFromPointNormal() {
		PA = p[225];
		RunASM();
	}
	void PROXY_D3DXPlaneFromPoints() {
		PA = p[226];
		RunASM();
	}
	void PROXY_D3DXPlaneIntersectLine() {
		PA = p[227];
		RunASM();
	}
	/*
	void PROXY_D3DXPlaneNormalize() {
		PA = p[228];
		RunASM();
	}
	void PROXY_D3DXPlaneTransform() {
		PA = p[229];
		RunASM();
	}*/
	void PROXY_D3DXPlaneTransformArray() {
		PA = p[230];
		RunASM();
	}
	void PROXY_D3DXPreprocessShader() {
		PA = p[231];
		RunASM();
	}
	void PROXY_D3DXPreprocessShaderFromFileA() {
		PA = p[232];
		RunASM();
	}
	void PROXY_D3DXPreprocessShaderFromFileW() {
		PA = p[233];
		RunASM();
	}
	void PROXY_D3DXPreprocessShaderFromResourceA() {
		PA = p[234];
		RunASM();
	}
	void PROXY_D3DXPreprocessShaderFromResourceW() {
		PA = p[235];
		RunASM();
	}
	void PROXY_D3DXQuaternionBaryCentric() {
		PA = p[236];
		RunASM();
	}
	void PROXY_D3DXQuaternionExp() {
		PA = p[237];
		RunASM();
	}
	void PROXY_D3DXQuaternionInverse() {
		PA = p[238];
		RunASM();
	}
	void PROXY_D3DXQuaternionLn() {
		PA = p[239];
		RunASM();
	}
	void PROXY_D3DXQuaternionMultiply() {
		PA = p[240];
		RunASM();
	}
	void PROXY_D3DXQuaternionNormalize() {
		PA = p[241];
		RunASM();
	}
	void PROXY_D3DXQuaternionRotationAxis() {
		PA = p[242];
		RunASM();
	}
	void PROXY_D3DXQuaternionRotationMatrix() {
		PA = p[243];
		RunASM();
	}
	void PROXY_D3DXQuaternionRotationYawPitchRoll() {
		PA = p[244];
		RunASM();
	}
	void PROXY_D3DXQuaternionSlerp() {
		PA = p[245];
		RunASM();
	}
	void PROXY_D3DXQuaternionSquad() {
		PA = p[246];
		RunASM();
	}
	void PROXY_D3DXQuaternionSquadSetup() {
		PA = p[247];
		RunASM();
	}
	void PROXY_D3DXQuaternionToAxisAngle() {
		PA = p[248];
		RunASM();
	}
	void PROXY_D3DXRectPatchSize() {
		PA = p[249];
		RunASM();
	}
	void PROXY_D3DXSHAdd() {
		PA = p[250];
		RunASM();
	}
	void PROXY_D3DXSHDot() {
		PA = p[251];
		RunASM();
	}
	void PROXY_D3DXSHEvalConeLight() {
		PA = p[252];
		RunASM();
	}
	void PROXY_D3DXSHEvalDirection() {
		PA = p[253];
		RunASM();
	}
	void PROXY_D3DXSHEvalDirectionalLight() {
		PA = p[254];
		RunASM();
	}
	void PROXY_D3DXSHEvalHemisphereLight() {
		PA = p[255];
		RunASM();
	}
	void PROXY_D3DXSHEvalSphericalLight() {
		PA = p[256];
		RunASM();
	}
	void PROXY_D3DXSHMultiply2() {
		PA = p[257];
		RunASM();
	}
	void PROXY_D3DXSHMultiply3() {
		PA = p[258];
		RunASM();
	}
	void PROXY_D3DXSHMultiply4() {
		PA = p[259];
		RunASM();
	}
	void PROXY_D3DXSHMultiply5() {
		PA = p[260];
		RunASM();
	}
	void PROXY_D3DXSHMultiply6() {
		PA = p[261];
		RunASM();
	}
	void PROXY_D3DXSHPRTCompSplitMeshSC() {
		PA = p[262];
		RunASM();
	}
	void PROXY_D3DXSHPRTCompSuperCluster() {
		PA = p[263];
		RunASM();
	}
	void PROXY_D3DXSHProjectCubeMap() {
		PA = p[264];
		RunASM();
	}
	void PROXY_D3DXSHRotate() {
		PA = p[265];
		RunASM();
	}
	void PROXY_D3DXSHRotateZ() {
		PA = p[266];
		RunASM();
	}
	void PROXY_D3DXSHScale() {
		PA = p[267];
		RunASM();
	}
	void PROXY_D3DXSaveMeshHierarchyToFileA() {
		PA = p[268];
		RunASM();
	}
	void PROXY_D3DXSaveMeshHierarchyToFileW() {
		PA = p[269];
		RunASM();
	}
	void PROXY_D3DXSaveMeshToXA() {
		PA = p[270];
		RunASM();
	}
	void PROXY_D3DXSaveMeshToXW() {
		PA = p[271];
		RunASM();
	}
	void PROXY_D3DXSavePRTBufferToFileA() {
		PA = p[272];
		RunASM();
	}
	void PROXY_D3DXSavePRTBufferToFileW() {
		PA = p[273];
		RunASM();
	}
	void PROXY_D3DXSavePRTCompBufferToFileA() {
		PA = p[274];
		RunASM();
	}
	void PROXY_D3DXSavePRTCompBufferToFileW() {
		PA = p[275];
		RunASM();
	}
	void PROXY_D3DXSaveSurfaceToFileA() {
		PA = p[276];
		RunASM();
	}
	void PROXY_D3DXSaveSurfaceToFileInMemory() {
		PA = p[277];
		RunASM();
	}
	void PROXY_D3DXSaveSurfaceToFileW() {
		PA = p[278];
		RunASM();
	}
	void PROXY_D3DXSaveTextureToFileA() {
		PA = p[279];
		RunASM();
	}
	void PROXY_D3DXSaveTextureToFileInMemory() {
		PA = p[280];
		RunASM();
	}
	void PROXY_D3DXSaveTextureToFileW() {
		PA = p[281];
		RunASM();
	}
	void PROXY_D3DXSaveVolumeToFileA() {
		PA = p[282];
		RunASM();
	}
	void PROXY_D3DXSaveVolumeToFileInMemory() {
		PA = p[283];
		RunASM();
	}
	void PROXY_D3DXSaveVolumeToFileW() {
		PA = p[284];
		RunASM();
	}
	void PROXY_D3DXSimplifyMesh() {
		PA = p[285];
		RunASM();
	}
	void PROXY_D3DXSphereBoundProbe() {
		PA = p[286];
		RunASM();
	}
	void PROXY_D3DXSplitMesh() {
		PA = p[287];
		RunASM();
	}
	void PROXY_D3DXTessellateNPatches() {
		PA = p[288];
		RunASM();
	}
	void PROXY_D3DXTessellateRectPatch() {
		PA = p[289];
		RunASM();
	}
	void PROXY_D3DXTessellateTriPatch() {
		PA = p[290];
		RunASM();
	}
	void PROXY_D3DXTriPatchSize() {
		PA = p[291];
		RunASM();
	}
	void PROXY_D3DXUVAtlasCreate() {
		PA = p[292];
		RunASM();
	}
	void PROXY_D3DXUVAtlasPack() {
		PA = p[293];
		RunASM();
	}
	void PROXY_D3DXUVAtlasPartition() {
		PA = p[294];
		RunASM();
	}
	void PROXY_D3DXValidMesh() {
		PA = p[295];
		RunASM();
	}
	void PROXY_D3DXValidPatchMesh() {
		PA = p[296];
		RunASM();
	}
	void PROXY_D3DXVec2BaryCentric() {
		PA = p[297];
		RunASM();
	}
	void PROXY_D3DXVec2CatmullRom() {
		PA = p[298];
		RunASM();
	}
	void PROXY_D3DXVec2Hermite() {
		PA = p[299];
		RunASM();
	}
	void PROXY_D3DXVec2Normalize() {
		PA = p[300];
		RunASM();
	}
	void PROXY_D3DXVec2Transform() {
		PA = p[301];
		RunASM();
	}
	void PROXY_D3DXVec2TransformArray() {
		PA = p[302];
		RunASM();
	}
	void PROXY_D3DXVec2TransformCoord() {
		PA = p[303];
		RunASM();
	}
	void PROXY_D3DXVec2TransformCoordArray() {
		PA = p[304];
		RunASM();
	}
	void PROXY_D3DXVec2TransformNormal() {
		PA = p[305];
		RunASM();
	}
	void PROXY_D3DXVec2TransformNormalArray() {
		PA = p[306];
		RunASM();
	}
	void PROXY_D3DXVec3BaryCentric() {
		PA = p[307];
		RunASM();
	}
	void PROXY_D3DXVec3CatmullRom() {
		PA = p[308];
		RunASM();
	}
	void PROXY_D3DXVec3Hermite() {
		PA = p[309];
		RunASM();
	}
	/*
	void PROXY_D3DXVec3Normalize() {
		PA = p[310];
		RunASM();
	}*/
	void PROXY_D3DXVec3Project() {
		PA = p[311];
		RunASM();
	}
	void PROXY_D3DXVec3ProjectArray() {
		PA = p[312];
		RunASM();
	}
	void PROXY_D3DXVec3Transform() {
		PA = p[313];
		RunASM();
	}
	void PROXY_D3DXVec3TransformArray() {
		PA = p[314];
		RunASM();
	}
	/*
	void PROXY_D3DXVec3TransformCoord() {
		PA = p[315];
		RunASM();
	}*/
	void PROXY_D3DXVec3TransformCoordArray() {
		PA = p[316];
		RunASM();
	}
	/*
	void PROXY_D3DXVec3TransformNormal() {
		PA = p[317];
		RunASM();
	}*/
	void PROXY_D3DXVec3TransformNormalArray() {
		PA = p[318];
		RunASM();
	}
	void PROXY_D3DXVec3Unproject() {
		PA = p[319];
		RunASM();
	}
	void PROXY_D3DXVec3UnprojectArray() {
		PA = p[320];
		RunASM();
	}
	void PROXY_D3DXVec4BaryCentric() {
		PA = p[321];
		RunASM();
	}
	void PROXY_D3DXVec4CatmullRom() {
		PA = p[322];
		RunASM();
	}
	void PROXY_D3DXVec4Cross() {
		PA = p[323];
		RunASM();
	}
	void PROXY_D3DXVec4Hermite() {
		PA = p[324];
		RunASM();
	}
	void PROXY_D3DXVec4Normalize() {
		PA = p[325];
		RunASM();
	}
	void PROXY_D3DXVec4Transform() {
		PA = p[326];
		RunASM();
	}
	void PROXY_D3DXVec4TransformArray() {
		PA = p[327];
		RunASM();
	}
	void PROXY_D3DXWeldVertices() {
		PA = p[328];
		RunASM();
	}
}
