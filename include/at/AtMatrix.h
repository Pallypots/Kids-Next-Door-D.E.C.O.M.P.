// The matrix functions that the table g_atMatrix (include/at/AtCoordSys.h) points to. Original address clusters 0x00105440-0x0010558F (SSE) and 0x00114FB0-0x00117862 (x87). All names are inferred, from what
// each function does and from what its callers pass.
//
// The table at 0x002C60C8 has 104 slots, every one written by 0x000EC2B0, called from 0x000EC670 for this table and for a second one at 0x002C7C48. Nothing else in the executable writes a slot. AtMatrixTable
// names 39: the 15 the recovered code calls, and 24 more that some code in the game reads. Of the 104, 65 are not recovered: 60 that no code in the executable reads, and 5 read somewhere, the rotations about
// an axis 0x00117EE0, 0x00117F40, 0x00117FA0 and 0x00118100 (through 0x001155F0 and 0x00115850) and the matrix to quaternion 0x001163D0. 0x000EC2B0 is not recovered as a function: g_atMatrix is initialised with
// the same targets instead. tests/oracle/oracle_matrix.cpp runs the original 0x000EC2B0 and calls through each slot it wrote, against the member of g_atMatrix.
//
// A matrix is sixteen floats, row by row: three axes and then a position, a point being a row vector multiplied on the left. Each function writes only what it says; the rest is left as it was.
//
// Arithmetic: the x87 functions keep intermediates in registers and round to float where they store, some of those stores being spills. The source shows that: `double` for what stays in a register, `float`
// for what was stored. With SSE arithmetic that gives the original's bits at x87 precision 53, the game's main thread's (docs/portability.md), provided the compiler does not contract a multiplication and an
// addition into one: see docs/worklog/2026-11-08-matrix.md. Copy and SetProduct are the original's SSE code, single precision throughout; they are written in `float`.
#pragma once

class AtVector3;
class AtQuaternion;

struct AtMatrix {
    float m[16];  // +0x00 rows: x axis 0-2, y axis 4-6, z axis 8-10, position 12-14; 3, 7, 11 and 15 the fourth column
};

// The original's Copy and SetProduct use movaps, so the original's matrices are 16-byte aligned or those fault. These do not need it.
void AtMatrixCopy(AtMatrix* to, const AtMatrix* from);                          // 0x00105440 slot Copy. Bits as they are, a signalling NaN too
void AtMatrixCopyWords(AtMatrix* to, const AtMatrix* from);                     // 0x00114FB0 the same, word by word; in no slot, Multiply's
void AtMatrixIdentity(AtMatrix* m);                                             // 0x00115020 slot Identity
void AtMatrixSetTranslation(AtMatrix* m, const AtVector3* position);            // 0x001150A0 slot SetTranslation. Only 12-14, bits as they are
void AtMatrixSetRotation(AtMatrix* m, const AtQuaternion* rotation);            // 0x001150F0 slot SetRotation. Only the 3x3, normalising by dividing by the squared length: a zero one gives infinities and NaNs
void AtMatrixSetScale(AtMatrix* m, const AtVector3* scale);                     // 0x00115370 slot SetScale. Every element, bits as they are
void AtMatrixSetUniformScale(AtMatrix* m, float scale);                         // 0x00115320 slot SetUniformScale. 0 through an x87 register, so a signalling NaN is quieted there and not in 5 and 10
// 0x00116150 slot SetInverse. Of a rotation and a translation only: the 3x3 transposed, the position turned back through it. The fourth column becomes 0, 0, 0, 1. Writes the position first, so `to` must not be
// `from`: then the original reads elements it has already written, which this keeps.
void AtMatrixSetInverse(AtMatrix* to, const AtMatrix* from);
void AtMatrixSetProductWords(AtMatrix* to, const AtMatrix* a, const AtMatrix* b);  // 0x001166A0 to = a b, x87, writing as it goes, so `to` must be neither; in no slot, Multiply's
void AtMatrixMultiply(AtMatrix* m, const AtMatrix* by);                         // 0x00116A20 slot Multiply. m = by m: on the left. by must not be m, or it is read as it is written
void AtMatrixSetProduct(AtMatrix* to, const AtMatrix* a, const AtMatrix* b);    // 0x00105470 slot SetProduct. to = a b in single precision; to may be a or b. Not the same bits as Multiply's
void AtMatrixTransformPoint(const AtMatrix* m, const AtVector3* from, AtVector3* to);      // 0x00116E30 slot TransformPoint. Rounds at every step, and reads from after writing to
void AtMatrixTransformDirection(const AtMatrix* m, const AtVector3* from, AtVector3* to);  // 0x00116FA0 slot TransformDirection. The same with no position
void AtMatrixGetPosition(const AtMatrix* m, AtVector3* position);              // 0x00116540 slot GetPosition
void AtMatrixGetAxes(const AtMatrix* m, AtVector3* x, AtVector3* y, AtVector3* z);  // 0x00116560 slot GetAxes
// 0x00117780 slot SetLookAt. x axis the direction normalised, z axis the up with its part along the direction taken away, normalised; y axis z cross x. Only the 3x3.
void AtMatrixSetLookAt(AtMatrix* m, const AtVector3* direction, const AtVector3* up);
void AtMatrixSetLookAtZY(AtMatrix* m, const AtVector3* direction, const AtVector3* up);  // 0x00117870 slot SetLookAtZY. The same axes, the direction as z, the up as y, x = y cross z
void AtMatrixSetLookAtXY(AtMatrix* m, const AtVector3* direction, const AtVector3* up);  // 0x00117950 slot SetLookAtXY. The direction as x, the up as y, z = x cross y

// Further slots the game calls, though nothing recovered yet does. Slot names are members of AtMatrixTable.
void AtMatrixSetScale3x3(AtMatrix* m, const AtVector3* scale);                  // 0x001150C0 slot SetScale3x3. The 3x3 only
void AtMatrixSetAxes(AtMatrix* m, const AtVector3* x, const AtVector3* y, const AtVector3* z);   // 0x001151F0 slot SetAxes. The 3x3 only, by rows
void AtMatrixMakeTranslation(AtMatrix* m, const AtVector3* position);           // 0x00115240 slot MakeTranslation. Identity and a position
void AtMatrixMakeRotation(AtMatrix* m, const AtQuaternion* rotation);           // 0x00115290 slot MakeRotation. SetRotation, then the fourth column 0, 0, 0, 1 and no position
void AtMatrixMakeFromAxes(AtMatrix* m, const AtVector3* x, const AtVector3* y, const AtVector3* z);  // 0x001152C0 slot MakeFromAxes
void AtMatrixSetAxisAngle(AtMatrix* m, const AtVector3* axis, float angle);     // 0x00117690 slot SetAxisAngle. Through AtQuaternion::SetAxisAngle 0x00033B90 and SetRotation
void AtMatrixMakeAxisAngle(AtMatrix* m, const AtVector3* axis, float angle);    // 0x00117A40 slot MakeAxisAngle. And as MakeRotation
void AtMatrixSetElement(AtMatrix* m, int row, int column, float value);        // 0x00116390 slot SetElement. Through a register, so a signalling NaN is quieted
float AtMatrixGetElement(const AtMatrix* m, int row, int column);              // 0x001163B0 slot GetElement. Returned in st(0)
void AtMatrixGetXAxis(const AtMatrix* m, AtVector3* axis);                      // 0x00116640 slot GetXAxis
void AtMatrixGetYAxis(const AtMatrix* m, AtVector3* axis);                      // 0x00116660 slot GetYAxis
void AtMatrixGetZAxis(const AtMatrix* m, AtVector3* axis);                      // 0x00116680 slot GetZAxis
void AtMatrixMultiplyRight(AtMatrix* m, const AtMatrix* by);                    // 0x00116A50 slot MultiplyRight. m = m by, x87; by must not be m
// 0x00116EB0 slot TransformPointZ. The z of TransformPoint, summed in another order and left in st(0) unrounded: so a double here, which a caller rounds where the original's stores it.
double AtMatrixTransformPointZ(const AtMatrix* m, const AtVector3* point);
void AtMatrixInverseTransformPoint(const AtMatrix* m, const AtVector3* from, AtVector3* to);      // 0x00116EE0 slot InverseTransformPoint. Through the transpose, for a rotation and a translation
void AtMatrixInverseTransformDirection(const AtMatrix* m, const AtVector3* from, AtVector3* to);  // 0x00117010 slot InverseTransformDirection
void AtMatrixScaleAxes(AtMatrix* m, const AtVector3* scale);                    // 0x00105780 slot ScaleAxes. SSE: rows 0-2, all four of each, by x, y and z. The original reads a fourth float past scale
void AtMatrixTranspose(AtMatrix* m);                                            // 0x001056A0 slot Transpose. SSE, all sixteen
void AtMatrixScaleColumnX(AtMatrix* m, float scale);                            // 0x00115B00 slot ScaleColumnX. 0, 4, 8, 12
void AtMatrixScaleColumnY(AtMatrix* m, float scale);                            // 0x00115B30 slot ScaleColumnY. 1, 5, 9, 13
// 0x001160C0 slot Invert. SetInverse in place, with its own order: the fourth column is left as it was, and 1 and 2 reach 4 and 8 through registers, quieting a signalling NaN.
void AtMatrixInvert(AtMatrix* m);
void AtMatrixTranslateAfter(AtMatrix* m, const AtVector3* by);                  // 0x001157B0 slot TranslateAfter. Each row gains by times its fourth element: m = m T
