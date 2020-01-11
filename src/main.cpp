#define PIN 5
#define NUM_LEDS 300
#include <Adafruit_NeoPixel_ZeroDMA.h>
//#include <Adafruit_NeoPixel.h>
#include "sprite.h"

using TStripType = Adafruit_NeoPixel_ZeroDMA;
//using TStripType = Adafruit_NeoPixel;
TStripType Strip(NUM_LEDS, PIN, NEO_GRB + NEO_KHZ800);

template <typename T, const unsigned int N>
constexpr unsigned int countof(T (&)[N]) { return N; }

void setup() {
    SerialUSB.begin(9600);
    Strip.begin();
    Strip.setBrightness(50);
}

class TActor {
public:
    unsigned long Period = 1000; // ms
    unsigned long LastDrawTime = 0;

    virtual ~TActor() = default;
    virtual void Draw(TStripType&) = 0;
    virtual void Move(TStripType&) = 0;

    bool IsTime() const {
        return millis() - LastDrawTime >= Period;
    }

    void UpdateTime() {
        LastDrawTime = millis();
    }

    void PostponeTime(uint32_t ahead) {
        LastDrawTime = millis() + ahead;
    }
};

union TColorRGB {
    uint32_t Value;
    struct {
        uint32_t B:8;
        uint32_t G:8;
        uint32_t R:8;
    };
};

class TColorSmoother {
public:
    static uint32_t MergeColors(uint32_t a, uint32_t b, float amount_b) {
        float amount_a = 1 - min(amount_b, 1);
        TColorRGB _a;
        TColorRGB _b;
        TColorRGB _r;

        _a.Value = a;
        _b.Value = b;
        _r.Value = 0;
        _r.R = min(round(_a.R * amount_a) + round(_b.R * amount_b), 255);
        _r.G = min(round(_a.G * amount_a) + round(_b.G * amount_b), 255);
        _r.B = min(round(_a.B * amount_a) + round(_b.B * amount_b), 255);
        return _r.Value;
    }

    static void SmoothApply(TStripType& strip, uint32_t pixelsDesired[NUM_LEDS], float trans) {
        auto pixels = strip.numPixels();
        strip.setPixelColor(0, MergeColors(pixelsDesired[0], pixelsDesired[pixels - 1], trans));
        for (unsigned int i = 1; i < pixels; ++i) {
            strip.setPixelColor(i, MergeColors(pixelsDesired[i], pixelsDesired[i - 1], trans));
        }
    }

    template <typename PatternType>
    static void MaskPattern(const PatternType& patternSource, PatternType& patternTarget, uint32_t patternMask) {
        TColorRGB mask;
        mask.Value = patternMask;
        for (unsigned int i = 0; i < countof(patternSource); ++i) {
            TColorRGB target;
            TColorRGB source;
            source.Value = patternSource[i];
            target.R = source.R * mask.R / 255;
            target.G = source.G * mask.G / 255;
            target.B = source.B * mask.B / 255;
            patternTarget[i] = target.Value;
        }
    }
};

template <typename T, int S>
T GetRandom(const T(&choices)[S]) {
    return choices[random(S)];
}

template <typename T, int S>
void MakeRandom(T& result, const T(&choices)[S]) {
    T r;
    do {
        r = GetRandom<T, S>(choices);
    } while (result == r);
    result = r;
}

template <typename T>
void MakeRandom(T& result, const T(&choices)[1]) {
    result = choices[0];
}

template <typename PatternType>
class TPatternActor : public TActor {
public:
    TPatternActor(const PatternType& pattern, int step = 1, bool repeat = false, int space = 0)
        : Pattern(pattern)
        , Step(step)
        , Repeat(repeat)
        , Space(space)
    {
        Period = 50;
    }

    virtual void Draw(TStripType& strip) override {
        auto pixels = strip.numPixels();
        if (Repeat) {
            for (unsigned int i = 0; i < pixels; ++i) {
                strip.setPixelColor((I + i) % pixels, Pattern[i % countof(Pattern)]);
                if (Space && (i % countof(Pattern)) == countof(Pattern) - 1) {
                    i += Space;
                }
            }
        } else {
            for (unsigned int i = 0; i < countof(Pattern); ++i) {
                strip.setPixelColor((I + i) % pixels, Pattern[i]);
            }
        }
    }

    virtual void Move(TStripType& strip) override {
        if (IsTime()) {
            I = (I + Step) % NUM_LEDS;
            UpdateTime();
        }
        Draw(strip);
    }

protected:
    const PatternType& Pattern;
    int Step;
    bool Repeat;
    int Space;
    int I = 0;
};

template <typename PatternType>
class TSmoothPatternActor : public TActor, TColorSmoother {
public:
    static constexpr int SMOOTH_LEVEL = 20;

    TSmoothPatternActor(const PatternType& pattern, bool repeat = false)
        : Pattern(pattern)
        , Repeat(repeat)
    {
        Period = 1;
    }

    virtual void Draw(TStripType& strip) override {
        float trans = float(S) / SMOOTH_LEVEL;
        SmoothApply(strip, PixelsDesired, trans);
    }

    virtual void Move(TStripType& strip) override {
        if (IsTime()) {
            S = (S + 1) % SMOOTH_LEVEL;
            if (S == 0) {
                I = (I + 1) % NUM_LEDS;
                auto pixels = strip.numPixels();
                if (Repeat) {
                    for (unsigned int i = 0; i < pixels; ++i) {
                        PixelsDesired[(I + i) % pixels] = Pattern[i % countof(Pattern)];
                    }
                } else {
                    for (unsigned int i = 0; i < countof(Pattern); ++i) {
                        PixelsDesired[(I + i) % pixels] = Pattern[i];
                    }
                }
            }
            UpdateTime();
        }
        Draw(strip);
    }

protected:
    const PatternType& Pattern;
    uint32_t PixelsDesired[NUM_LEDS];
    bool Repeat;
    int I = 0;
    int S = 0;
};

template <typename PatternType>
class TChaoticPatternMovementActor : public TActor {
public:
    TChaoticPatternMovementActor(const PatternType& pattern)
        : Pattern(pattern)
    {
        Period = 1;
    }

    virtual void Draw(TStripType& strip) override {
        auto pixels = strip.numPixels();
        for (unsigned int i = 0; i < countof(Pattern); ++i) {
            strip.setPixelColor((I + i) % pixels, Pattern[i]);
        }
    }

    virtual void Move(TStripType& strip) override {
        if (IsTime()) {
            Period = 1;
            I = (I + Step) % NUM_LEDS;
            if (I == D) {
                D = random(NUM_LEDS);
                if (D > I) {
                    Step = 1;
                } else if (D < I) {
                    Step = -1;
                } else {
                    Step = 0;
                }
                Period = 100;
            }
            UpdateTime();
        }
        Draw(strip);
    }

protected:
    const PatternType& Pattern;
    int I = 0;
    int Step = 0;
    int D = 0;
};

template <typename PatternType>
class TChaoticPatternMovementWithRandomTrailActor : public TActor {
public:
    TChaoticPatternMovementWithRandomTrailActor(const PatternType& pattern)
        : Pattern(pattern)
    {
        Period = 1;
    }

    virtual void Draw(TStripType& strip) override {
        auto pixels = strip.numPixels();
        for (unsigned int i = 0; i < countof(Pattern); ++i) {
            strip.setPixelColor((I + i) % pixels, Pattern[i]);
        }
        if (Step < 0) {
            strip.setPixelColor((I + countof(Pattern) + 1) % pixels, Trail);
        }
        if (Step > 0) {
            strip.setPixelColor((I + pixels - 1) % pixels, Trail);
        }
    }

    virtual void Move(TStripType& strip) override {
        if (IsTime()) {
            Period = 1;
            if (D > I) {
                Step = 1;
            } else if (D < I) {
                Step = -1;
            }
            I = (I + Step) % NUM_LEDS;
            if (I == D) {
                D = random(NUM_LEDS);
                uint32_t color = 0;
                color |= random(0x10);
                color <<= 8;
                color |= random(0x10);
                color <<= 8;
                color |= random(0x10);
                Trail = color;
                Period = 10;
            }
            UpdateTime();
        }
        Draw(strip);
    }

protected:
    const PatternType& Pattern;
    int I = 0;
    int Step = 0;
    int D = 0;
    uint32_t Trail = 0;
};

class TRandomFillActor : public TActor {
    uint32_t Pixels[NUM_LEDS];

public:
    TRandomFillActor() {
        Period = 5000;
    }

    virtual void Draw(TStripType& strip) override {
        for (unsigned int i = 0; i < countof(Pixels); ++i) {
            strip.setPixelColor(i, Pixels[i]);
        }        
    }

    virtual void Move(TStripType& strip) override {
        if (IsTime()) {
            for (unsigned int i = 0; i < countof(Pixels); ++i) {
                uint32_t color = 0;
                color |= random(256);
                color <<= 8;
                color |= random(256);
                color <<= 8;
                color |= random(256);
                Pixels[i] = color;
            }
            UpdateTime();
        }
        Draw(strip);
    }
};

class TRandomShifterActor : public TActor {
    uint32_t Pixels[NUM_LEDS];

public:
    TRandomShifterActor() {
        Period = 5;
    }

    virtual void Draw(TStripType& strip) override {
        for (unsigned int i = 0; i < countof(Pixels); ++i) {
            strip.setPixelColor(i, Pixels[i]);
        }        
    }

    virtual void Move(TStripType& strip) override {
        if (IsTime()) {
            for (unsigned int i = NUM_LEDS - 1; i > 0; --i) {
                Pixels[i] = Pixels[i - 1];
            }
            uint32_t color = 0;
            color |= random(256);
            color <<= 8;
            color |= random(256);
            color <<= 8;
            color |= random(256);
            Pixels[0] = color;
            UpdateTime();
        }
        Draw(strip);
    }
};

template <typename ColorsType>
class TRandomSelectorShifterActor : public TActor {
    uint32_t Pixels[NUM_LEDS];

public:
    TRandomSelectorShifterActor(const ColorsType& colors)
        : Colors(colors)
    {
        Period = 10;
        for (unsigned int i = 0; i < NUM_LEDS; ++i) {
            Pixels[i] = GetRandom(Colors);
        }
    }

    virtual void Draw(TStripType& strip) override {
        for (unsigned int i = 0; i < countof(Pixels); ++i) {
            strip.setPixelColor(i, Pixels[i]);
        }        
    }

    virtual void Move(TStripType& strip) override {
        if (IsTime()) {
            auto s = Pixels[NUM_LEDS - 1];
            for (unsigned int i = NUM_LEDS - 1; i > 0; --i) {
                Pixels[i] = Pixels[i - 1];
            }
            Pixels[0] = s;
            UpdateTime();
        }
        Draw(strip);
    }

protected:
    const ColorsType& Colors;
};

template <typename ColorsType>
class TRandomSelectorSmoothShifterActor : public TActor, TColorSmoother {
    uint32_t PixelsDesired[NUM_LEDS];
    static constexpr int MAX_SHIFT = 10;
    int Shift = 0;

public:
    TRandomSelectorSmoothShifterActor(const ColorsType& colors)
        : Colors(colors)
    {
        Period = 10;
        for (unsigned int i = 0; i < NUM_LEDS; ++i) {
            PixelsDesired[i] = GetRandom(Colors);
        }
    }

    virtual void Draw(TStripType& strip) override {
        float trans = float(Shift) / MAX_SHIFT;
        SmoothApply(strip, PixelsDesired, trans);
    }

    virtual void Move(TStripType& strip) override {
        if (IsTime()) {
            Shift = (Shift + 1) % MAX_SHIFT;
            if (Shift == 0) {
                auto s = PixelsDesired[NUM_LEDS - 1];
                for (unsigned int i = NUM_LEDS - 1; i > 0; --i) {
                    PixelsDesired[i] = PixelsDesired[i - 1];
                }
                PixelsDesired[0] = s;
            }
            UpdateTime();
        }
        Draw(strip);
    }

protected:
    const ColorsType& Colors;
};

template <typename ColorsType>
class TRandomSmoothBlenderActor : public TActor, TColorSmoother {
    uint32_t Pixels[NUM_LEDS];
    uint32_t PixelsDesired[NUM_LEDS];
    static constexpr int MAX_SHIFT = 50;
    int Shift = 0;

public:
    TRandomSmoothBlenderActor(const ColorsType& colors, TStripType& strip)
        : Colors(colors)
    {
        Period = 100;
        for (unsigned int i = 0; i < NUM_LEDS; ++i) {
            Pixels[i] = strip.getPixelColor(i);
            PixelsDesired[i] = GetRandom(Colors);
        }
    }

    virtual void Draw(TStripType& strip) override {
        float trans = float(Shift) / MAX_SHIFT;
        for (unsigned int i = 0; i < NUM_LEDS; ++i) {
            strip.setPixelColor(i, MergeColors(Pixels[i], PixelsDesired[i], trans));
        }
    }

    virtual void Move(TStripType& strip) override {
        if (IsTime()) {
            Shift = (Shift + 1) % MAX_SHIFT;
            if (Shift == 0) {
                for (unsigned int i = 0; i < NUM_LEDS; ++i) {
                    Pixels[i] = PixelsDesired[i];
                    PixelsDesired[i] = GetRandom(Colors);
                }
            }
            UpdateTime();
        }
        Draw(strip);
    }

protected:
    const ColorsType& Colors;
};

template <typename ColorsType>
class TSingleRandomSmoothBlenderActor : public TActor, TColorSmoother {
    uint32_t Pixels[NUM_LEDS];
    uint32_t ColorDesired;
    static constexpr int MAX_SHIFT = 250;
    int Shift = 0;

public:
    TSingleRandomSmoothBlenderActor(const ColorsType& colors, TStripType& strip)
        : Colors(colors)
    {
        Period = 10;
        ColorDesired = GetRandom(Colors);
        for (unsigned int i = 0; i < NUM_LEDS; ++i) {
            Pixels[i] = strip.getPixelColor(i);
        }
    }

    virtual void Draw(TStripType& strip) override {
        float trans = float(Shift) / (MAX_SHIFT - 1);
        for (unsigned int i = 0; i < NUM_LEDS; ++i) {
            strip.setPixelColor(i, MergeColors(Pixels[i], ColorDesired, trans));
        }
    }

    virtual void Move(TStripType& strip) override {
        if (IsTime()) {
            Shift = (Shift + 1) % MAX_SHIFT;
            if (Shift == 0) {
                for (unsigned int i = 0; i < NUM_LEDS; ++i) {
                    Pixels[i] = ColorDesired;
                }
                MakeRandom(ColorDesired, Colors);
                PostponeTime(10000);
            } else {
                UpdateTime();
            }
        }
        Draw(strip);
    }

protected:
    const ColorsType& Colors;
};

class TSingleColorGradientActor : public TActor, TColorSmoother {
    uint32_t ColorDesired;

public:
    TSingleColorGradientActor(uint32_t color)
        : ColorDesired(color)
    {
    }

    virtual void Draw(TStripType& strip) override {
        for (unsigned int i = 0; i < NUM_LEDS; ++i) {
            strip.setPixelColor(i, MergeColors(0, ColorDesired, float(i) / NUM_LEDS));
        }
    }

    virtual void Move(TStripType& strip) override {
        if (IsTime()) {
            UpdateTime();
        }
        Draw(strip);
    }
};

template <typename ColorsType>
class TDecayingSplashesActor : public TActor {
public:
    TDecayingSplashesActor(int amount, int speed, const ColorsType& colors, TStripType& strip)
        : Amount(amount)
        , Speed(speed)
        , Colors(colors)
    {
        Period = 5;
        for (unsigned int i = 0; i < NUM_LEDS; ++i) {
            PixelsDesired[i].Value = strip.getPixelColor(i);
        }
    }

    virtual void Draw(TStripType& strip) override {
        for (unsigned int i = 0; i < NUM_LEDS; ++i) {
            strip.setPixelColor(i, PixelsDesired[i].Value);
        }
    }

    virtual void Move(TStripType& strip) override {
        if (IsTime()) {
            for (unsigned i = 0; i < NUM_LEDS; ++i) {
                PixelsDesired[i].R -= min(PixelsDesired[i].R, Speed);
                PixelsDesired[i].G -= min(PixelsDesired[i].G, Speed);
                PixelsDesired[i].B -= min(PixelsDesired[i].B, Speed);
            }
            for (int i = 0; i < Amount; ++i) {
                PixelsDesired[random(NUM_LEDS)].Value = GetRandom(Colors);
            }
            UpdateTime();
        }
        Draw(strip);
    }

protected:
    TColorRGB PixelsDesired[NUM_LEDS] = {};
    int Amount;
    int Speed;
    const ColorsType& Colors;
};

class TSingleColorActor : public TActor, TColorSmoother {
public:
    TSingleColorActor(uint32_t color)
        : Color(color)
    {
        Period = 1000;
    }

    virtual void Draw(TStripType& strip) override {
        for (unsigned int i = 0; i < NUM_LEDS; ++i) {
            strip.setPixelColor(i, Color);
        }
    }

    virtual void Move(TStripType& strip) override {
        if (IsTime()) {
            UpdateTime();
        }
        Draw(strip);
    }

protected:
    uint32_t Color;
};

template <typename ColorsType>
class TShiftRandomColorsActor : public TActor, TColorSmoother {
public:
    TShiftRandomColorsActor(const ColorsType& colors)
        : Colors(colors)
    {
        Period = 50;
        Color = GetRandom(Colors);
    }

    virtual void Draw(TStripType& strip) override {
        for (int i = 0; i < NUM_LEDS; i += Distance) {
            if (Pos % 2 == 0) {
                strip.setPixelColor(i + Distance / 2 + Pos / 2, Color);
            } else {
                strip.setPixelColor(i + Distance / 2 - Pos / 2 - 1, Color);
            }
        }
    }

    virtual void Move(TStripType& strip) override {
        Draw(strip);
        if (IsTime()) {
            ++Pos;
            if (Pos >= Distance) {
                Pos = 0;
                MakeRandom(Color, Colors);
            }
            UpdateTime();
        }
    }

protected:
    const ColorsType& Colors;
    int Pos = 0;
    int Distance = 50;
    uint32_t Color;
};

template <typename ColorsType>
class TProportionalColorsActor : public TActor, TColorSmoother {
public:
    TProportionalColorsActor(const ColorsType& colors)
        : Colors(colors)
    {
        Period = 1000;
    }

    virtual void Draw(TStripType& strip) override {
        if (countof(Colors) > 1) {
            float colorSize = float(NUM_LEDS) / (countof(Colors) - 1);
            for (unsigned int i = 0; i < NUM_LEDS; ++i) {
                unsigned int colorIndex = i / colorSize;
                strip.setPixelColor(i, MergeColors(Colors[colorIndex], Colors[colorIndex + 1], (i - colorSize * colorIndex) / colorSize));
            }
        } else {
            for (unsigned int i = 0; i < NUM_LEDS; ++i) {
                strip.setPixelColor(i, Colors[0]);
            }
        }
    }

    virtual void Move(TStripType& strip) override {
        if (IsTime()) {
            UpdateTime();
        }
        Draw(strip);
    }

protected:
    const ColorsType& Colors;
};

template <typename AnimationType, int Count>
class TAnimationActor : TActor {
public:
    TAnimationActor(const AnimationType& animation)
        : Animation(animation)
    {
        Period = 20;
    }

    virtual void Draw(TStripType& strip) override {
        uint32_t time = millis();
        for (unsigned int i = 0; i < Count; ++i) {
            int position = Positions[i];
            const auto* image = Animations[i].GetCurrentImage(Animation, time);
            if (image) {
                for (unsigned int p = 0; p < Animation.GetSize(); ++p) {
                    strip.setPixelColor(position + p, (*image)[p]);
                }
            }
        }
    }

    virtual void Move(TStripType& strip) override {
        if (IsTime()) {
            Animations[AnimationNum].Start(millis());
            Positions[AnimationNum] = random(NUM_LEDS - Animation.GetSize() + 1);
            AnimationNum = (AnimationNum + 1) % Count;
            UpdateTime();
        }
        Draw(strip);
    }

protected:
    const AnimationType& Animation;
    TAnimationPlay Animations[Count];
    int Positions[Count];
    int AnimationNum = 0;
};

//uint32_t Pattern[] = {0x400040, 0x800080, 0xC000C0, 0xFF00FF};
//uint32_t Pattern[] = {0x000000, 0xFFFFFF};
uint32_t Pattern[] = {0x000000, 0x010101, 0x101010, 0x202020, 0x404040, 0x808080, 0xC0C0C0, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF,
                      0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xC0C0C0, 0x808080, 0x404040, 0x202020, 0x101010, 0x010101, 0x000000};
uint32_t ChaoticPattern[] = {0x000000, 0x010101, 0x101010, 0x404040, 0x101010, 0x010101, 0x000000};

//uint32_t Colors[] = {0xFF0000, 0x00FF00, 0x0000FF};
uint32_t Colors[] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0x00FFFF, 0xFF00FF, /*pink*/0xFFC0CB, /*orange*/0xFFA500};
uint32_t WhiteColor[] = {0xFFFFFF};
uint32_t RainbowColors[] = {0xFF0000, 0xFF7F00, 0xFFFF00, 0x00FF00, 0x0000FF, 0x2E2B5F, 0x8B00FF};

TAnimation<10, 9> Animation1 = {
    {
        {8, {0x000000, 0x000000, 0x000000, 0x000000, 0xFFFFFF, 0x000000, 0x000000, 0x000000, 0x000000}},
        {8, {0x000000, 0x000000, 0x000000, 0xC0C0C0, 0xFFFFFF, 0xC0C0C0, 0x000000, 0x000000, 0x000000}},
        {8, {0x000000, 0x000000, 0x000000, 0xFFFFFF, 0xC0C0C0, 0xFFFFFF, 0x000000, 0x000000, 0x000000}},
        {5, {0x000000, 0x000000, 0xC0C0C0, 0xC0C0C0, 0x808080, 0xC0C0C0, 0xC0C0C0, 0x000000, 0x000000}},
        {5, {0x000000, 0x000000, 0xC0C0C0, 0x808080, 0x404040, 0x808080, 0xC0C0C0, 0x000000, 0x000000}},
        {5, {0x000000, 0x000000, 0x808080, 0x404040, 0x202020, 0x404040, 0x808080, 0x000000, 0x000000}},
        {8, {0x000000, 0x404040, 0x202020, 0x101010, 0x080808, 0x101010, 0x202020, 0x404040, 0x000000}},
        {8, {0x202020, 0x101010, 0x080808, 0x040404, 0x040004, 0x040404, 0x080808, 0x101010, 0x202020}},
        {8, {0x101010, 0x080808, 0x040404, 0x040004, 0x040004, 0x000004, 0x040404, 0x080808, 0x101010}},
        {8, {0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000}}
    }
};

/*TPatternActor<decltype(Pattern)> PatternActor(Pattern, 1, true, 40);
TSmoothPatternActor<decltype(Pattern)> SmoothPatternActor(Pattern, true);
TChaoticPatternMovementActor<decltype(ChaoticPattern)> ChaoticPatternMovementActor(ChaoticPattern);
TChaoticPatternMovementWithRandomTrailActor<decltype(ChaoticPattern)> ChaoticPatternMovementWithRandomTrailActor(ChaoticPattern);
TRandomFillActor RandomFillActor;
TRandomShifterActor RandomShifterActor;
TRandomSelectorShifterActor<decltype(Colors)> RandomSelectorShifterActor(Colors);
TRandomSelectorSmoothShifterActor<decltype(Colors)> RandomSelectorSmoothShifterActor(Colors);
TRandomSmoothBlenderActor<decltype(Colors)> RandomSmoothBlenderActor(Colors);
TDecayingSplashesActor<decltype(WhiteColor)> DecayingSplashesActor(1, 5, WhiteColor);
TProportionalColorsActor<decltype(RainbowColors)> ProportionalColorsActor(RainbowColors);
TAnimationActor<decltype(Animation1), 10> AnimationActor(Animation1);*/

TActor* CurrentActor = nullptr;
uint32_t StrategyStartTime = 0;
static constexpr uint32_t STRATEGY_TIME = 30000;
decltype(Pattern) PatternCopy;
uint32_t SingleColor[1];
int Strategy = -1;

uint32_t last = 0;
void loop() {
    unsigned long now = millis();
    if (now > StrategyStartTime + STRATEGY_TIME || CurrentActor == nullptr) {
        int choice;
        do {
            choice = random(6);
        } while (choice == Strategy);
        SerialUSB.print("Switching to strategy ");
        SerialUSB.println(choice);
        Strategy = choice;
        delete CurrentActor;
        switch(Strategy) {
            case 0:
                TColorSmoother::MaskPattern(Pattern, PatternCopy, GetRandom(Colors));
                CurrentActor = new TPatternActor<decltype(PatternCopy)>(PatternCopy, 1, true, 40);
                break;
            case 1:
                CurrentActor = new TDecayingSplashesActor<decltype(Colors)>(1, 5, Colors, Strip);
                break;
            case 2:
                MakeRandom(SingleColor[0], Colors);
                CurrentActor = new TDecayingSplashesActor<decltype(SingleColor)>(1, 5, SingleColor, Strip);
                break;
            case 3:
                CurrentActor = new TSingleRandomSmoothBlenderActor<decltype(Colors)>(Colors, Strip);
                break;
            case 4:
                CurrentActor = new TShiftRandomColorsActor<decltype(Colors)>(Colors);
                break;
            default:
                CurrentActor = new TRandomSelectorSmoothShifterActor<decltype(Colors)>(Colors);
                break;
        }
        StrategyStartTime = now;
    }
    CurrentActor->Move(Strip);
    //RandomSmoothBlenderActor.Move(Strip);
    //RandomSelectorShifterActor.Move(Strip);
    //RandomSelectorSmoothShifterActor.Move(Strip);
    //PatternActor.Move(Strip);
    //ChaoticPatternMovementActor.Move(Strip);
    //ChaoticPatternMovementWithRandomTrailActor.Move(Strip);
    //PatternActor.Move(Strip);
    //DecayingSplashesActor.Move(Strip);
    //ProportionalColorsActor.Move(Strip);
    //AnimationActor.Move(Strip);
    Strip.show();
    if (SerialUSB.available()) {
        auto cmd = SerialUSB.read();
        switch (cmd) {
        case 'p': {
            for (unsigned int i = 0; i < NUM_LEDS; ++i) {
                int32_t color = Strip.getPixelColor(i);
                SerialUSB.print(color, HEX);
                if (i % 16 == 15) {
                    SerialUSB.println();
                } else {
                    SerialUSB.print(' ');
                }
            }
            SerialUSB.println();
            break;
        }
        case 'r':
            SerialUSB.println("red");
            delete CurrentActor;
            SingleColor[0] = 0xFF0000;
            CurrentActor = new TSingleRandomSmoothBlenderActor<decltype(SingleColor)>(SingleColor, Strip);
            break;
        case 'g':
            SerialUSB.println("green");
            delete CurrentActor;
            SingleColor[0] = 0x00FF00;
            CurrentActor = new TSingleRandomSmoothBlenderActor<decltype(SingleColor)>(SingleColor, Strip);
            break;
        case 'b':
            SerialUSB.println("blue");
            delete CurrentActor;
            SingleColor[0] = 0x0000FF;
            CurrentActor = new TSingleRandomSmoothBlenderActor<decltype(SingleColor)>(SingleColor, Strip);
            break;
        case 'w':
            SerialUSB.println("white");
            delete CurrentActor;
            SingleColor[0] = 0xFFFFFF;
            CurrentActor = new TSingleRandomSmoothBlenderActor<decltype(SingleColor)>(SingleColor, Strip);
            break;
        case 'i':
            SerialUSB.println("pink");
            delete CurrentActor;
            SingleColor[0] = 0xFFC0CB;
            CurrentActor = new TSingleRandomSmoothBlenderActor<decltype(SingleColor)>(SingleColor, Strip);
            break;
        case 'R':
            SerialUSB.println("RED");
            delete CurrentActor;
            CurrentActor = new TSingleColorGradientActor(0xFF0000);
            break;
        case 'G':
            SerialUSB.println("GREEN");
            delete CurrentActor;
            CurrentActor = new TSingleColorGradientActor(0x00FF00);
            break;
        case 'B':
            SerialUSB.println("BLUE");
            delete CurrentActor;
            CurrentActor = new TSingleColorGradientActor(0x0000FF);
            break;
        case 'W':
            SerialUSB.println("WHITE");
            delete CurrentActor;
            CurrentActor = new TSingleColorGradientActor(0xFFFFFF);
            break;
        case 'I':
            SerialUSB.println("PINK");
            delete CurrentActor;
            CurrentActor = new TSingleColorGradientActor(0xFFC0CB);
            break;
        case 'n':
            delete CurrentActor;
            CurrentActor = nullptr;
            break;
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9': {
            int brightness = (cmd - '0') * 255 / 9;
            SerialUSB.print("Setting brightness to ");
            SerialUSB.println(brightness);
            Strip.setBrightness(brightness);
            break;   
        }
        default:
            SerialUSB.println("Unknown command");
            break;
        }
        StrategyStartTime = now;
    }
}
