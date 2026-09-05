// =============================================================================
// src/resource/ResourceManager.cpp — SFX / BGM / Font / Texture 캐시 구현.
// =============================================================================

#include "ResourceManager.h"

#include <SDL_image.h>
#include <SDL_mixer.h>
#include <SDL_ttf.h>

Mix_Chunk* ResourceManager::LoadSfx(const std::string& key, const std::string& path) {
    auto it = sfx_.find(key);
    if (it != sfx_.end()) return it->second.get();
    Mix_Chunk* raw = Mix_LoadWAV(path.c_str());
    if (!raw) return nullptr;
    sfx_.emplace(key, UniqueChunk(raw, Mix_FreeChunk));
    return raw;
}

Mix_Music* ResourceManager::LoadBgm(const std::string& key, const std::string& path) {
    auto it = bgm_.find(key);
    if (it != bgm_.end()) return it->second.get();
    Mix_Music* raw = Mix_LoadMUS(path.c_str());
    if (!raw) return nullptr;
    bgm_.emplace(key, UniqueMusic(raw, Mix_FreeMusic));
    return raw;
}

TTF_Font* ResourceManager::LoadFont(const std::string& key, const std::string& path, int size) {
    // 같은 폰트의 다른 size 가 별도 인스턴스 — 키에 ":size" 붙여 분리.
    const std::string fkey = key + ":" + std::to_string(size);
    auto it = fonts_.find(fkey);
    if (it != fonts_.end()) return it->second.get();
    TTF_Font* raw = TTF_OpenFont(path.c_str(), size);
    if (!raw) return nullptr;
    fonts_.emplace(fkey, UniqueFont(raw, TTF_CloseFont));
    return raw;
}

SDL_Texture* ResourceManager::LoadTexture(const std::string& key,
                                          const std::string& path,
                                          SDL_Renderer* renderer) {
    // 캐시 hit — 이미 GPU 에 업로드된 텍스처 포인터 그대로 반환.
    auto it = textures_.find(key);
    if (it != textures_.end()) return it->second.get();

    // 캐시 miss — IMG_LoadTexture 로 PNG/BMP/JPG 등 디스크 파일을 읽어
    // SDL_Renderer 가 관리하는 GPU 텍스처로 업로드. 메인 스레드 전용 호출.
    SDL_Texture* raw = IMG_LoadTexture(renderer, path.c_str());
    if (!raw) return nullptr;  // 자산 부재 정책: 캐시 미등록, nullptr 반환.

    textures_.emplace(key, UniqueTexture(raw, SDL_DestroyTexture));
    return raw;
}

Mix_Chunk* ResourceManager::GetSfx(const std::string& key) const {
    auto it = sfx_.find(key);
    return (it == sfx_.end()) ? nullptr : it->second.get();
}

Mix_Music* ResourceManager::GetBgm(const std::string& key) const {
    auto it = bgm_.find(key);
    return (it == bgm_.end()) ? nullptr : it->second.get();
}

TTF_Font* ResourceManager::GetFont(const std::string& key, int size) const {
    const std::string fkey = key + ":" + std::to_string(size);
    auto it = fonts_.find(fkey);
    return (it == fonts_.end()) ? nullptr : it->second.get();
}

SDL_Texture* ResourceManager::GetTexture(const std::string& key) const {
    // 캐시 조회 전용 — LoadTexture 를 먼저 호출하지 않은 키는 nullptr 반환.
    auto it = textures_.find(key);
    return (it == textures_.end()) ? nullptr : it->second.get();
}

void ResourceManager::ClearAll() {
    // unique_ptr 의 deleter (Mix_FreeChunk / Mix_FreeMusic / TTF_CloseFont /
    // SDL_DestroyTexture) 가 각 unordered_map clear 시점에 호출 → SDL 자원
    // 한 번에 해제.
    sfx_.clear();
    bgm_.clear();
    fonts_.clear();
    textures_.clear();  // GPU 텍스처 해제 (SDL_DestroyTexture).
}
