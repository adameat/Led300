#pragma once

template <int Size>
struct TSprite {
    using ImageType = uint32_t[Size];
    uint32_t Duration;
    ImageType Image;
};

template <int Count, int Size>
struct TAnimation {
    using ImageType = typename TSprite<Size>::ImageType;
    TSprite<Size> Sprites[Count];

    static constexpr int GetCount() {
        return Count;
    }

    static constexpr int GetSize() {
        return Size;
    }

    uint32_t GetTotalDuration() const {
        uint32_t duration = 0;
        for (const auto& sprite : Sprites) {
            duration += sprite.Duration;
        }
        return duration;
    }
};

struct TAnimationPlay {
    uint32_t StartTime;

    TAnimationPlay()
        : StartTime(0)
    {}

    void Start(uint32_t time) {
        StartTime = time;
    }

    template <typename AnimationType>
    const typename AnimationType::ImageType* GetCurrentImage(const AnimationType& animation, uint32_t time) const {
        uint32_t delta = time - StartTime;
        for (unsigned int i = 0; i < animation.GetCount(); ++i) {
            if (delta < animation.Sprites[i].Duration) {
                return &animation.Sprites[i].Image;
            }
            delta -= animation.Sprites[i].Duration;
        }
        return nullptr;
    }
};
