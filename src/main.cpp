#define PIN 6
#define NUM_LEDS 300
#include "Adafruit_NeoPixel.h"

Adafruit_NeoPixel Strip(NUM_LEDS, PIN, NEO_GRB + NEO_KHZ800);
using TStripType = Adafruit_NeoPixel;

template <typename T, const unsigned int N>
constexpr unsigned int countof(T (&)[N]) { return N; }

void setup() {
    Strip.begin();
    Strip.setBrightness(255);
    pinMode(13, OUTPUT);
}

class TActor {
public:
    unsigned long Period = 1000; // ms
    unsigned long LastDrawTime = 0;

    virtual void Draw(TStripType&) = 0;
    virtual void Move(TStripType&) = 0;

    bool IsTime() const {
        return millis() - LastDrawTime >= Period;
    }

    void UpdateTime() {
        LastDrawTime = millis();
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
        float amount_a = 1 - amount_b;
        TColorRGB _a;
        TColorRGB _b;
        TColorRGB _r;

        _a.Value = a;
        _b.Value = b;
        _r.Value = 0;
        _r.R = int(_a.R * amount_a) + int(_b.R * amount_b);
        _r.G = int(_a.G * amount_a) + int(_b.G * amount_b);
        _r.B = int(_a.B * amount_a) + int(_b.B * amount_b);
        return _r.Value;
    }

    static void SmoothApply(TStripType& strip, uint32_t pixelsDesired[NUM_LEDS], float trans) {
        auto pixels = strip.numPixels();
        strip.setPixelColor(0, MergeColors(pixelsDesired[0], pixelsDesired[pixels - 1], trans));
        for (unsigned int i = 1; i < pixels; ++i) {
            strip.setPixelColor(i, MergeColors(pixelsDesired[i], pixelsDesired[i - 1], trans));
        }
    }
};

template <typename PatternType>
class TPatternActor : TActor {
public:
    TPatternActor(const PatternType& pattern, int step = 1, bool repeat = false, int space = 0)
        : Pattern(pattern)
        , Step(step)
        , Repeat(repeat)
        , Space(space)
    {
        Period = 5;
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
class TSmoothPatternActor : TActor, TColorSmoother {
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
class TChaoticPatternMovementActor : TActor {
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
class TChaoticPatternMovementWithRandomTrailActor : TActor {
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

class TRandomFillActor : TActor {
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

class TRandomShifterActor : TActor {
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
class TRandomSelectorShifterActor : TActor {
    uint32_t Pixels[NUM_LEDS];

public:
    TRandomSelectorShifterActor(const ColorsType& colors)
        : Colors(colors)
    {
        Period = 50;
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
            Pixels[0] = Colors[random(countof(Colors))];
            UpdateTime();
        }
        Draw(strip);
    }

protected:
    const ColorsType& Colors;
};

template <typename ColorsType>
class TRandomSelectorSmoothShifterActor : TActor, TColorSmoother {
    uint32_t PixelsDesired[NUM_LEDS];
    static constexpr int MAX_SHIFT = 20;
    int Shift = 0;

public:
    TRandomSelectorSmoothShifterActor(const ColorsType& colors)
        : Colors(colors)
    {
        Period = 10;
        for (unsigned int i = 0; i < NUM_LEDS; ++i) {
            PixelsDesired[i] = Colors[random(countof(Colors))];
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
class TDecayingSplashesActor : TActor {
public:
    TDecayingSplashesActor(int amount, int speed, const ColorsType& colors)
        : Amount(amount)
        , Speed(speed)
        , Colors(colors)
    {
        Period = 5;
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
                PixelsDesired[random(NUM_LEDS)].Value = Colors[random(countof(Colors))];
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

template <typename ColorsType>
class TProportionalColorsActor : TActor, TColorSmoother {
public:
    TProportionalColorsActor(const ColorsType& colors)
        : Colors(colors)
    {
        Period = 1000;
    }

    virtual void Draw(TStripType& strip) override {
        float colorSize = float(NUM_LEDS) / (countof(Colors) - 1);
        for (unsigned int i = 0; i < NUM_LEDS; ++i) {
            unsigned int colorIndex = i / colorSize;
            strip.setPixelColor(i, MergeColors(Colors[colorIndex], Colors[colorIndex + 1], (i - colorSize * colorIndex) / colorSize));
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


//uint32_t Pattern[] = {0x400040, 0x800080, 0xC000C0, 0xFF00FF};
//uint32_t Pattern[] = {0x000000, 0xFFFFFF};
uint32_t Pattern[] = {0x000000, 0x010001, 0x100010, 0x200020, 0x400040, 0x800080, 0xC000C0, 0xFF00FF, 0xFF00FF, 0xFF00FF,
                      0xFF00FF, 0xFF00FF, 0xFF00FF, 0xC000C0, 0x800080, 0x400040, 0x200020, 0x100010, 0x010001, 0x000000};
TPatternActor<decltype(Pattern)> PatternActor(Pattern, 1, true, 40);
TSmoothPatternActor<decltype(Pattern)> SmoothPatternActor(Pattern, true);
uint32_t ChaoticPattern[] = {0x010101, 0x101010, 0x404040, 0x101010, 0x010101};
TChaoticPatternMovementActor<decltype(ChaoticPattern)> ChaoticPatternMovementActor(ChaoticPattern);
TChaoticPatternMovementWithRandomTrailActor<decltype(ChaoticPattern)> ChaoticPatternMovementWithRandomTrailActor(ChaoticPattern);
TRandomFillActor RandomFillActor;
TRandomShifterActor RandomShifterActor;
//uint32_t Colors[] = {0xFF0000, 0x00FF00, 0x0000FF};
uint32_t Colors[] = {0xFF0000, 0x00FF00, 0x0000FF, 0xFFFF00, 0x00FFFF, 0xFF00FF};
TRandomSelectorShifterActor<decltype(Colors)> RandomSelectorShifterActor(Colors);
TRandomSelectorSmoothShifterActor<decltype(Colors)> RandomSelectorSmoothShifterActor(Colors);
uint32_t WhiteColor[] = {0xFFFFFF};
TDecayingSplashesActor<decltype(WhiteColor)> DecayingSplashesActor(1, 5, WhiteColor);
uint32_t RainbowColors[] = {0xFF0000, 0xFF7F00, 0xFFFF00, 0x00FF00, 0x0000FF, 0x2E2B5F, 0x8B00FF};
TProportionalColorsActor<decltype(RainbowColors)> ProportionalColorsActor(RainbowColors);

void loop() {
    //RandomSelectorSmoothShifterActor.Move(Strip);
    //PatternActor.Move(Strip);
    //ChaoticPatternMovementActor.Move(Strip);
    //ChaoticPatternMovementWithRandomTrailActor.Move(Strip);
    //PatternActor.Move(Strip);
    //DecayingSplashesActor.Move(Strip);
    ProportionalColorsActor.Move(Strip);
    Strip.show();
}
