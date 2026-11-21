// See include/at/AtGr2D.h. What the fatal error screen draws with: rectangles, colours, an image's strips, the camera, and the comparison of names it chooses the language by.
// Verification status per function: config/functions.d/fatal.toml.
#include "at/AtGr2D.h"

void AtFatal(const char* message, int file, int line, int unused);

namespace {

void NotRecovered(const char* what) {
    AtFatal(what, 0, 0, 0);
}

const float kByteToReal = 0.0039215689f;  // 0x00272D4C, 0x3B808081: 1/255
const float kRealToByte = 255.0f;         // 0x00276204
const float kHalfPixel = 0.5f;            // 0x0027463C
const float kQuarterPixel = 0.25f;        // 0x00277158

// ORIGINAL: a real made an integer through the C library's _ftol2, which truncates to 64 bits; the byte stored is the low one of those. So a value past an int's range but
// within a 64-bit one keeps its low bits, where a plain (int) would give 0x80000000; NaN and what is past 64 bits give 0.
long long Truncate(double value) {
    return (long long)value;
}

}  // namespace

// ---- names -------------------------------------------------------------------------------------------------------------

int AtCompareName8(const char* a, const char* b) {
    if (a == 0) {
        a = "";
    }
    if (b == 0) {
        b = "";
    }
    int left = 8;
    for (;;) {
        const char x = *a;
        const char y = *b;
        left--;
        if (x != y) {
            return (signed char)*a - (signed char)*b;
        }
        if (x == 0) {
            return 0;
        }
        a++;
        b++;
        if (left == 0) {
            return 0;
        }
    }
}

// ---- rectangles --------------------------------------------------------------------------------------------------------

int AtRect::Clip(const AtRect& clip, const AtRect& rect, AtRect* uv) {
    enum { kLeft = 1, kTop = 2, kRight = 4, kBottom = 8 };
    unsigned int cut = 0;
    if (clip.left > rect.left) {
        left = clip.left;
        cut |= kLeft;
    } else {
        left = rect.left;
    }
    if (clip.right < rect.right) {
        right = clip.right;
        cut |= kRight;
    } else {
        right = rect.right;
    }
    if (right < left || right == left) {  // ORIGINAL: so a NaN on either side goes on
        return 0;
    }
    if (clip.top > rect.top) {
        top = clip.top;
        cut |= kTop;
    } else {
        top = rect.top;
    }
    if (clip.bottom < rect.bottom) {
        bottom = clip.bottom;
        cut |= kBottom;
    } else {
        bottom = rect.bottom;
    }
    if (bottom < top || bottom == top) {
        return 0;
    }
    if (cut != 0) {
        // The width and the height stay in a register; each side is in terms of uv's left or top as they stand, so the right is done before the left.
        const double width = (double)uv->right - uv->left;
        if (cut & kRight) {
            uv->right = (float)(((double)right - rect.left) / ((double)rect.right - rect.left) * width + uv->left);
        }
        if (cut & kLeft) {
            uv->left = (float)(((double)left - rect.left) / ((double)rect.right - rect.left) * width + uv->left);
        }
        const double height = (double)uv->bottom - uv->top;
        if (cut & kBottom) {
            uv->bottom = (float)(((double)bottom - rect.top) / ((double)rect.bottom - rect.top) * height + uv->top);
        }
        if (cut & kTop) {
            uv->top = (float)(((double)top - rect.top) / ((double)rect.bottom - rect.top) * height + uv->top);
        }
    }
    return 1;
}

// ---- colours -----------------------------------------------------------------------------------------------------------

void AtColor::Lerp(const AtColor& from, const AtColor& to, float t) {
    r = (unsigned char)Truncate((double)(to.r - from.r) * t + from.r);
    g = (unsigned char)Truncate((double)(to.g - from.g) * t + from.g);
    b = (unsigned char)Truncate((double)(to.b - from.b) * t + from.b);
    a = (unsigned char)Truncate((double)(to.a - from.a) * t + from.a);
}

void AtColor::LerpReal(const AtColorF& from, const AtColorF& to, float t) {
    AtColor first;
    first.r = (unsigned char)Truncate((double)from.r * kRealToByte);
    first.g = (unsigned char)Truncate((double)from.g * kRealToByte);
    first.b = (unsigned char)Truncate((double)from.b * kRealToByte);
    first.a = (unsigned char)Truncate((double)from.a * kRealToByte);
    AtColor second;
    second.r = (unsigned char)Truncate((double)to.r * kRealToByte);
    second.g = (unsigned char)Truncate((double)to.g * kRealToByte);
    second.b = (unsigned char)Truncate((double)to.b * kRealToByte);
    second.a = (unsigned char)Truncate((double)to.a * kRealToByte);
    Lerp(first, second, t);
}

void AtColorF::SetBytes(const AtColor& color) {
    r = (float)((double)color.r * kByteToReal);
    g = (float)((double)color.g * kByteToReal);
    b = (float)((double)color.b * kByteToReal);
    a = (float)((double)color.a * kByteToReal);
}

void AtColorF::Lerp(const AtColorF& from, const AtColorF& to, float t) {
    *this = to;
    r = (float)((double)r - from.r);
    g = (float)((double)g - from.g);
    b = (float)((double)b - from.b);
    a = (float)((double)a - from.a);
    // ORIGINAL: the products of red and green stay in registers for the sums; those of blue and alpha are stored and read back, so they are rounded first.
    const double redProduct = (double)t * r;
    r = (float)redProduct;
    const double greenProduct = (double)t * g;
    g = (float)greenProduct;
    b = (float)((double)t * b);
    a = (float)((double)t * a);
    r = (float)(redProduct + from.r);
    g = (float)(greenProduct + from.g);
    b = (float)((double)b + from.b);
    a = (float)((double)a + from.a);
}

void AtColorF::LerpBytes(const AtColor& from, const AtColor& to, float t) {
    AtColorF first;
    first.SetBytes(from);
    AtColorF second;
    second.SetBytes(to);
    Lerp(first, second, t);
}

void AtColorRectangle::Clip(const AtColorRectangle& colors, const AtRect& full, const AtRect& part) {
    AtColorF left;
    AtColorF right;
    if (part.top == full.top) {
        left.SetBytes(colors.m_colors[0]);
        right.SetBytes(colors.m_colors[1]);
    } else {
        const float down = (float)(((double)part.top - full.top) / ((double)full.bottom - full.top));
        left.LerpBytes(colors.m_colors[0], colors.m_colors[2], down);
        right.LerpBytes(colors.m_colors[1], colors.m_colors[3], down);
    }
    const float across0 = (float)(((double)part.left - full.left) / ((double)full.right - full.left));
    m_colors[0].LerpReal(left, right, across0);
    const float across1 = (float)(((double)part.right - full.left) / ((double)full.right - full.left));
    m_colors[1].LerpReal(left, right, across1);
    if (part.bottom == full.bottom) {
        left.SetBytes(colors.m_colors[2]);
        right.SetBytes(colors.m_colors[3]);
    } else {
        const float down = (float)(((double)part.bottom - full.top) / ((double)full.bottom - full.top));
        left.LerpBytes(colors.m_colors[0], colors.m_colors[2], down);
        right.LerpBytes(colors.m_colors[1], colors.m_colors[3], down);
    }
    m_colors[2].LerpReal(left, right, across0);
    m_colors[3].LerpReal(left, right, across1);
}

// ---- an image ----------------------------------------------------------------------------------------------------------

void AtImage::Draw(AtGr2DRenderer* renderer, const AtVector2* at, const AtVector2* scale, const AtColorRectangle* colors, const AtRect* clip) {
    AtColorRectangle flat;
    if ((m_flags & kOwnsData) == 0) {  // ORIGINAL: this bit, which the concrete image may use otherwise
        return;
    }
    AtVector2 one;
    if (scale == 0) {
        one.x = 1.0f;
        one.y = 1.0f;
        scale = &one;
    }
    AtRect full;  // the whole image: what a colour rectangle's corners stand for
    full.left = at->x;
    full.top = at->y;
    full.right = (float)((double)m_unknown14 * scale->x + full.left);
    full.bottom = (float)((double)m_unknown16 * scale->y + full.top);
    AtRect screen;
    if (clip == 0) {
        screen.left = (float)renderer->m_viewport[0];
        screen.top = (float)renderer->m_viewport[1];
        screen.right = (float)renderer->m_viewport[2];
        screen.bottom = (float)renderer->m_viewport[3];
        clip = &screen;
    }
    if (colors == 0 && (m_flags & kColorRectangle) != 0) {
        colors = &m_colorRectangle;
    }
    AtMaterial* material = renderer->m_material;
    if (material == 0) {
        material = Material();
    }
    if (colors == 0) {
        AtColor color;
        if (material != 0) {
            color = material->m_constant;
        } else {
            color.SetWhite();
        }
        flat.SetAll(color);
    }

    const AtImageStrip* strips = (const AtImageStrip*)m_data;
    const int rowStart = strips[0].x0;
    AtRect where;
    where.left = at->x;
    where.top = at->y;
    where.bottom = at->y;
    for (unsigned int i = 0; i < m_unknown1E; i++) {
        const AtImageStrip& strip = strips[i];
        if (strip.x0 == rowStart) {  // a new row, below the last strip drawn or not
            where.left = at->x;
            where.top = where.bottom;
        }
        const int width = strip.x1 - strip.x0;
        where.right = (float)((double)width * scale->x + where.left);
        const int height = strip.y1 - strip.y0;
        AtRect uv;
        uv.left = 0.0f;
        uv.top = 0.0f;
        where.bottom = (float)((double)height * scale->y + where.top);
        uv.right = (float)width;
        uv.bottom = (float)height;
        AtRect shown;
        if (shown.Clip(*clip, where, &uv) != 0) {
            const float y = (renderer->m_flags & AtGr2DRenderer::kQuarterPixelY) != 0 ? kQuarterPixel : kHalfPixel;
            shown.top = (float)((double)shown.top - y);
            shown.bottom = (float)((double)shown.bottom - y);
            const float x = (renderer->m_flags & AtGr2DRenderer::kQuarterPixelX) != 0 ? kQuarterPixel : kHalfPixel;
            shown.left = (float)((double)shown.left - x);
            shown.right = (float)((double)shown.right - x);
            if ((m_flags & kFlipVertically) != 0) {
                const float swap = shown.top;
                shown.top = shown.bottom;
                shown.bottom = swap;
            }
            if ((m_flags & kFlipHorizontally) != 0) {
                const float swap = shown.left;
                shown.left = shown.right;
                shown.right = swap;
            }
            if (colors != 0) {
                flat.Clip(*colors, full, shown);
            }
            DrawStrip(renderer, i, material, &shown, &uv, &flat);
            where.left = where.right;  // ORIGINAL: only after a strip that showed, so one wholly clipped leaves the next where it was
        }
    }
}

// ---- the camera --------------------------------------------------------------------------------------------------------

void AtCamera::Attach(AtMaterialOwner* owner) {
    if (m_matrix30 != 0) {
        Reset();
    }
    m_owner = owner;
    AtMatrixDevice* device = owner->MatrixDevice();
    m_matrix2C = device->NewMatrix8();
    m_matrix30 = device->NewMatrix8();
    AtMatrix* world = device->NewMatrix8();
    AtMatrix* local = device->NewMatrix8();
    SetMatrices(local, world);
}

// ---- the graphics layer's 2D renderer: not recovered -------------------------------------------------------------------

void AtGr2DRenderer::Begin(AtCamera*, int, int) { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 3, begin)"); }
void AtGr2DRenderer::End() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 62, end)"); }
void AtGr2DRenderer::Slot0() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 0)"); }
void AtGr2DRenderer::Slot1() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 1)"); }
void AtGr2DRenderer::Slot2() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 2)"); }
void AtGr2DRenderer::Slot4() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 4)"); }
void AtGr2DRenderer::Slot5() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 5)"); }
void AtGr2DRenderer::Slot6() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 6)"); }
void AtGr2DRenderer::Slot7() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 7)"); }
void AtGr2DRenderer::Slot8() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 8)"); }
void AtGr2DRenderer::Slot9() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 9)"); }
void AtGr2DRenderer::Slot10() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 10)"); }
void AtGr2DRenderer::Slot11() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 11)"); }
void AtGr2DRenderer::Slot12() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 12)"); }
void AtGr2DRenderer::Slot13() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 13)"); }
void AtGr2DRenderer::Slot14() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 14)"); }
void AtGr2DRenderer::Slot15() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 15)"); }
void AtGr2DRenderer::Slot16() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 16)"); }
void AtGr2DRenderer::Slot17() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 17)"); }
void AtGr2DRenderer::Slot18() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 18)"); }
void AtGr2DRenderer::Slot19() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 19)"); }
void AtGr2DRenderer::Slot20() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 20)"); }
void AtGr2DRenderer::Slot21() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 21)"); }
void AtGr2DRenderer::Slot22() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 22)"); }
void AtGr2DRenderer::Slot23() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 23)"); }
void AtGr2DRenderer::Slot24() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 24)"); }
void AtGr2DRenderer::Slot25() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 25)"); }
void AtGr2DRenderer::Slot26() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 26)"); }
void AtGr2DRenderer::Slot27() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 27)"); }
void AtGr2DRenderer::Slot28() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 28)"); }
void AtGr2DRenderer::Slot29() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 29)"); }
void AtGr2DRenderer::Slot30() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 30)"); }
void AtGr2DRenderer::Slot31() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 31)"); }
void AtGr2DRenderer::Slot32() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 32)"); }
void AtGr2DRenderer::Slot33() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 33)"); }
void AtGr2DRenderer::Slot34() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 34)"); }
void AtGr2DRenderer::Slot35() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 35)"); }
void AtGr2DRenderer::Slot36() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 36)"); }
void AtGr2DRenderer::Slot37() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 37)"); }
void AtGr2DRenderer::Slot38() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 38)"); }
void AtGr2DRenderer::Slot39() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 39)"); }
void AtGr2DRenderer::Slot40() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 40)"); }
void AtGr2DRenderer::Slot41() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 41)"); }
void AtGr2DRenderer::Slot42() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 42)"); }
void AtGr2DRenderer::Slot43() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 43)"); }
void AtGr2DRenderer::Slot44() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 44)"); }
void AtGr2DRenderer::Slot45() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 45)"); }
void AtGr2DRenderer::Slot46() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 46)"); }
void AtGr2DRenderer::Slot47() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 47)"); }
void AtGr2DRenderer::Slot48() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 48)"); }
void AtGr2DRenderer::Slot49() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 49)"); }
void AtGr2DRenderer::Slot50() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 50)"); }
void AtGr2DRenderer::Slot51() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 51)"); }
void AtGr2DRenderer::Slot52() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 52)"); }
void AtGr2DRenderer::Slot53() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 53)"); }
void AtGr2DRenderer::Slot54() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 54)"); }
void AtGr2DRenderer::Slot55() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 55)"); }
void AtGr2DRenderer::Slot56() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 56)"); }
void AtGr2DRenderer::Slot57() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 57)"); }
void AtGr2DRenderer::Slot58() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 58)"); }
void AtGr2DRenderer::Slot59() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 59)"); }
void AtGr2DRenderer::Slot60() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 60)"); }
void AtGr2DRenderer::Slot61() { NotRecovered("Atlas Graphics: the 2D renderer of the graphics layer is not recovered (slot 61)"); }
