// A vector of three floats. Original address cluster 0x00042D80-0x0004340F. All names are inferred.
//
// Arithmetic: the original keeps its intermediates in x87 registers and rounds to float only where it stores, some of those being spills to the stack. The source shows that: `double` for what stays
// in a register, `float` for what the original stored. With SSE arithmetic that gives the original's bits at x87 precision 53, apart from the sine and cosine: see src/at/AtMathHost.cpp. The game's main thread
// runs at that precision: docs/portability.md.
// In source order, the order of the sums matters: it is the order the original adds in, which is not the same from one function to the next.
#pragma once

class AtIoTokenizer;

// Supplied by src/at/AtMathHost.cpp. The original uses fsin and fcos.
double AtSin(double angle);
double AtCos(double angle);
// 1 - cos(angle). Separate, since fcos gives more bits than a double holds, and near an angle of 0 those are what is left after the subtraction.
double AtOneMinusCos(double angle);

class AtVector3 {
public:
    void Lerp(const AtVector3& from, const AtVector3& to, float t);      // 0x00042D80
    float DistanceSquared(const AtVector3& other) const;                 // 0x00042DC0
    void Read(AtIoTokenizer* tokenizer);                                 // 0x00042DF0
    void Normalize();                                                    // 0x00042E20 no longer than 5e-6: becomes (1, 0, 0)
    void SetNormalized(const AtVector3& from);                           // 0x00042E80 likewise
    float NormalizeReturningLength();                                    // 0x00042EF0 0 when it was too short
    float Distance(const AtVector3& other) const;                        // 0x00042F60
    void Rotate(const AtVector3& axis, float angle);                     // 0x00042F90 about a unit axis
    // 0x000430A0. Writes as it goes, so from must not be this vector.
    void SetRotated(const AtVector3& axis, float angle, const AtVector3& from);
    void RotateZ(float angle);                                           // 0x000431B0
    // 0x000431E0. Onto a direction. With a zero direction only y and z are cleared: x stays.
    void Project(const AtVector3& direction);
    void SetProjected(const AtVector3& from, const AtVector3& direction);  // 0x00043250 with a zero direction: from's x, then 0, 0
    // 0x000432D0. Takes away the part along a direction. With a zero direction only x is cleared.
    void Reject(const AtVector3& direction);
    void SetRejected(const AtVector3& from, const AtVector3& direction);   // 0x00043370 with a zero direction: 0, then from's y and z

    float x;   // +0x00
    float y;   // +0x04
    float z;   // +0x08
};

#if (defined(_M_IX86) || defined(__i386__)) && (__cplusplus >= 201103L || (defined(_MSC_VER) && _MSC_VER >= 1900))
static_assert(sizeof(AtVector3) == 12, "AtVector3");
#endif
