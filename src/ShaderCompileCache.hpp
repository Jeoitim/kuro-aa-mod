// SPDX-License-Identifier: MIT
#pragma once
#include <Windows.h>
#include <bcrypt.h>
#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace kuro_shader_cache {
inline std::atomic<unsigned> memory_hits{0},disk_hits{0},misses{0};
inline std::mutex cache_mutex;
inline std::unordered_map<std::string,std::vector<char>> memory;
inline std::filesystem::path module_root(){
    HMODULE module=nullptr;wchar_t path[32768]{};
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&module_root),&module);
    GetModuleFileNameW(module,path,32768);return std::filesystem::path(path).parent_path();
}
inline bool option(const wchar_t *key){const auto path=module_root()/L"KuroUI.ini";return GetPrivateProfileIntW(L"KuroUI",key,1,path.c_str())!=0;}
inline std::filesystem::path directory(){
    wchar_t override_path[32768]{};
    if(GetEnvironmentVariableW(L"KURO_AA_SHADER_CACHE",override_path,32768))return override_path;
    wchar_t local[32768]{};if(!GetEnvironmentVariableW(L"LOCALAPPDATA",local,32768))return {};
    return std::filesystem::path(local)/L"KuroAA"/L"shader-cache-v1";
}
inline std::string sha256(const char *data,size_t size){
    BCRYPT_ALG_HANDLE algorithm=nullptr;unsigned char digest[32]{};
    if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0)return {};
    const NTSTATUS result=BCryptHash(algorithm,nullptr,0,reinterpret_cast<PUCHAR>(const_cast<char*>(data)),static_cast<ULONG>(size),digest,32);
    BCryptCloseAlgorithmProvider(algorithm,0);if(result<0)return {};
    const char *hex="0123456789abcdef";std::string value;value.reserve(64);
    for(unsigned char byte:digest){value+=hex[byte>>4];value+=hex[byte&15];}return value;
}
inline void field(std::string &key,const void *data,size_t size){key.append(reinterpret_cast<const char*>(&size),sizeof(size));key.append(static_cast<const char*>(data),size);}
inline bool valid(const std::vector<char> &bytes){
    if(bytes.size()<32 || bytes.size()>16*1024*1024 || std::memcmp(bytes.data(),"DXBC",4))return false;
    ID3D11ShaderReflection *reflection=nullptr;const HRESULT hr=D3DReflect(bytes.data(),bytes.size(),__uuidof(ID3D11ShaderReflection),reinterpret_cast<void**>(&reflection));
    if(reflection)reflection->Release();return SUCCEEDED(hr);
}
inline HRESULT blob(const std::vector<char> &bytes,ID3DBlob **result){
    const HRESULT hr=D3DCreateBlob(bytes.size(),result);if(SUCCEEDED(hr))std::memcpy((*result)->GetBufferPointer(),bytes.data(),bytes.size());return hr;
}
inline HRESULT compile(const void *source,SIZE_T size,LPCSTR name,const D3D_SHADER_MACRO *defines,ID3DInclude *include,
    LPCSTR entry,LPCSTR target,UINT flags,UINT flags2,ID3DBlob **code,ID3DBlob **errors){
    if(code)*code=nullptr;if(errors)*errors=nullptr;
    try {
        if(include || !source || !code || !entry || !target || !option(L"ShaderDiskCache"))return D3DCompile(source,size,name,defines,include,entry,target,flags,flags2,code,errors);
        std::string key;field(key,source,size);field(key,entry,std::strlen(entry));field(key,target,std::strlen(target));
        if(name)field(key,name,std::strlen(name));field(key,&flags,sizeof(flags));field(key,&flags2,sizeof(flags2));
        if(defines)for(auto macro=defines;macro->Name;++macro){field(key,macro->Name,std::strlen(macro->Name));const bool has=macro->Definition!=nullptr;field(key,&has,sizeof(has));if(has)field(key,macro->Definition,std::strlen(macro->Definition));}
        wchar_t compiler_path[32768]{};auto compiler=GetModuleHandleW(L"d3dcompiler_47.dll");
        if(compiler && GetModuleFileNameW(compiler,compiler_path,32768)){
            WIN32_FILE_ATTRIBUTE_DATA info{};if(GetFileAttributesExW(compiler_path,GetFileExInfoStandard,&info)){
                field(key,&info.ftLastWriteTime,sizeof(info.ftLastWriteTime));field(key,&info.nFileSizeLow,sizeof(info.nFileSizeLow));
            }
        }
        const auto hash=sha256(key.data(),key.size());if(hash.empty())return D3DCompile(source,size,name,defines,include,entry,target,flags,flags2,code,errors);
        std::lock_guard<std::mutex> guard(cache_mutex);
        auto found=memory.find(hash);if(found!=memory.end()){++memory_hits;return blob(found->second,code);}
        const auto root=directory();const auto path=root/(hash+".cso");std::vector<char> bytes;
        if(!root.empty()){
            std::ifstream file(path,std::ios::binary);uint32_t count=0;char signature[8]{},checksum[64]{};
            if(file.read(signature,8) && !std::memcmp(signature,"KAA-CS01",8) && file.read(reinterpret_cast<char*>(&count),4)
                && count>=32 && count<=16*1024*1024 && file.read(checksum,64)){
                bytes.resize(count);if(file.read(bytes.data(),count) && sha256(bytes.data(),bytes.size())==std::string(checksum,64) && valid(bytes)){
                    ++disk_hits;memory.emplace(hash,bytes);return blob(bytes,code);
                }
            }
        }
        ++misses;const HRESULT hr=D3DCompile(source,size,name,defines,include,entry,target,flags,flags2,code,errors);if(FAILED(hr))return hr;
        const char *start=static_cast<const char*>((*code)->GetBufferPointer());bytes.assign(start,start+(*code)->GetBufferSize());memory[hash]=bytes;
        if(!root.empty()){
            std::error_code ec;std::filesystem::create_directories(root,ec);
            if(!ec){const auto temp=std::filesystem::path(path.wstring()+L"."+std::to_wstring(GetCurrentProcessId())+L"."+std::to_wstring(GetCurrentThreadId())+L".tmp");
                std::ofstream file(temp,std::ios::binary|std::ios::trunc);uint32_t count=static_cast<uint32_t>(bytes.size());const auto checksum=sha256(bytes.data(),bytes.size());
                file.write("KAA-CS01",8);file.write(reinterpret_cast<const char*>(&count),4);file.write(checksum.data(),checksum.size());file.write(bytes.data(),bytes.size());file.close();
                if(file)MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH);else std::filesystem::remove(temp,ec);
            }
        }
        return hr;
    }catch(...){if(code && *code){(*code)->Release();*code=nullptr;}if(errors && *errors){(*errors)->Release();*errors=nullptr;}return D3DCompile(source,size,name,defines,include,entry,target,flags,flags2,code,errors);}
}
}
