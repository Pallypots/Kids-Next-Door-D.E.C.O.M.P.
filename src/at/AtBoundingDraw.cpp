// See include/at/AtBoundingGeometry.h. Drawing a box and a sphere, for showing bounding geometry while debugging, and the vertex format they ask for. Original address clusters 0x000454C0-0x00045586, 0x00055750-0x000557C7, 0x00058CB0-0x000594FF.
#include "AtBoundingMath.h"

using namespace AtBoundingMath;

namespace {

const float kPi = 3.14159274f;
const float kHalfPi = 1.57079637f;
const float kTwoPi = 6.28318548f;

inline AtVector3* Position(const AtGeometryLocked& locked, unsigned int index) {
    return (AtVector3*)((char*)locked.m_positions + (int)index * locked.m_positionStride);
}

void PutCorner(AtVector3* to, const AtBoundingBox* box, float sx, float sy, float sz) {
    to->x = (float)(D(box->m_center.x) + D(sx) * box->m_half.x);
    to->y = (float)(D(box->m_center.y) + D(sy) * box->m_half.y);
    to->z = (float)(D(box->m_center.z) + D(sz) * box->m_half.z);
}

// The original's unsigned to floating point, by way of a signed load and adding 2^32.
inline double FromUnsigned(unsigned int value) { return (double)value; }

}  // namespace

AtVertexAttributes::AtVertexAttributes() {
    m_flags = 0;
    m_count = 0;
    m_mask0 = 0;
    m_mask1 = 0;
    m_mask2 = 0;
    m_mask3 = 0;
}

void AtVertexAttributes::Set(unsigned int flags, unsigned int count) {
    if (count < 1) {
        m_mask1 = 0;
        flags &= 0xFFFFFDFFu;
    } else {
        m_mask1 = 0x1F;
        if (count > 4) {
            count = 4;
        }
        flags |= 0x200;
    }
    m_count = count;
    m_flags = flags;
    m_mask0 = 0x1F;
    m_mask2 = (unsigned char)(((flags >> 10) & 1) << 3);
    m_mask3 = (flags & 0x800) != 0 ? 0x1F : 0;
}

AtVertexFormat::AtVertexFormat() {
    m_texCoordCount = 0;
    for (int i = 0; i < 8; i++) {
        m_texCoordMasks[i] = 0;
    }
    m_mask4 = 0;
    m_mask5 = 0;
}

void AtVertexFormat::Set(unsigned int flags, unsigned int count, unsigned int texCoords) {
    if (texCoords < 1) {
        flags &= 0x40007FFFu;
    } else {
        if (texCoords > 8) {
            texCoords = 8;
        }
        flags |= 0x8000;
    }
    m_attributes.Set(flags, count);
    m_texCoordCount = texCoords;
    m_mask4 = (m_attributes.m_flags & 0x2000) != 0 ? 0x1E : 0;
    m_mask5 = (m_attributes.m_flags & 0x4000) != 0 ? 0x1E : 0;
    unsigned int i = 0;
    for (; i < texCoords; i++) {
        m_texCoordMasks[i] = 0x1F;
    }
    for (; i < 8; i++) {
        m_texCoordMasks[i] = 0;
    }
}

void AtGeometryDrawDevice::DrawBox(const AtBoundingBox* box, void* material, void* instance) {
    AtGeometryLocked locked;
    AtVertexFormat format;
    format.Set(0xA080, 0, 1);
    AtGeometryBuffer* buffer = m_source->Buffer();
    buffer->Lock(&format, 8, &locked);
    unsigned short* indices;
    buffer->LockIndices(36, &indices);

    static const float kCorners[8][3] = {{1, 1, 1}, {1, -1, 1}, {-1, 1, 1}, {-1, -1, 1}, {1, 1, -1}, {1, -1, -1}, {-1, 1, -1}, {-1, -1, -1}};
    static const float kTexCoords[8][2] = {{1, 0}, {1, 1}, {0, 0}, {0, 1}, {1, 0}, {1, 1}, {0, 0}, {0, 1}};
    for (unsigned int i = 0; i < 8; i++) {
        PutCorner(Position(locked, i), box, kCorners[i][0], kCorners[i][1], kCorners[i][2]);
        float* uv = (float*)((char*)locked.m_texCoords + (int)i * locked.m_texCoordStride);
        uv[0] = kTexCoords[i][0];
        uv[1] = kTexCoords[i][1];
    }
    static const unsigned short kFaces[36] = {0, 1, 2, 1, 3, 2, 3, 1, 7, 1, 5, 7, 1, 0, 5, 0, 4, 5, 0, 2, 4, 2, 6, 4, 2, 3, 6, 3, 7, 6, 6, 7, 4, 7, 5, 4};
    for (unsigned int i = 0; i < 36; i++) {
        indices[i] = kFaces[i];
    }
    buffer->UnlockIndices(36);
    buffer->Unlock(8);
    buffer->Draw(this, material, 1, &instance);
}

void AtGeometryDrawDevice::DrawSphere(const AtSceneSphere* sphere, void* material, unsigned int stacks, unsigned int slices, void* instance) {
    AtGeometryLocked locked;
    AtVertexFormat format;
    const unsigned int vertices = stacks * slices + 2;
    unsigned int indexCount = slices * 6;
    if (stacks > 1) {
        indexCount += (stacks - 1) * slices * 6;
    }
    format.Set(0x8080, 0, 1);
    AtGeometryBuffer* buffer = m_source->Buffer();
    buffer->Lock(&format, vertices, &locked);
    unsigned short* index;
    buffer->LockIndices(indexCount, &index);

    const double radius = sphere->radius;
    AtVector3* top = Position(locked, 0);
    top->x = sphere->center.x;
    top->y = sphere->center.y;
    top->z = (float)(D(sphere->center.z) + radius);
    unsigned int written = 1;
    const float step = (float)(D(kPi) / FromUnsigned(stacks + 1));

    unsigned int ringStart = 0;  // Where the ring being joined to the one before starts.
    for (unsigned int stack = 0; stack < stacks; stack++) {
        const double latitude = D(kHalfPi) - FromUnsigned(stack + 1) * step;
        if (slices != 0) {
            const float sliceCount = (float)FromUnsigned(slices);
            const float sinLatitude = (float)AtSin(latitude);
            const double cosLatitude = AtCos(latitude);
            for (unsigned int slice = 0; slice < slices; slice++) {
                const double angle = FromUnsigned(slice) * kTwoPi / sliceCount;
                const float stored = (float)angle;
                // The sine is of the angle as it stood in the register, and the cosine of it stored: they are not quite one angle.
                AtVector3* to = Position(locked, written + slice);
                to->y = (float)(AtSin(angle) * cosLatitude * radius + sphere->center.y);
                to->z = (float)(D(sinLatitude) * radius + sphere->center.z);
                to->x = (float)(AtCos(stored) * cosLatitude * radius + sphere->center.x);
            }
            written += slices;
        }
        for (unsigned int slice = 0; slice < slices; slice++) {
            if (stack == 0) {
                index[0] = 0;
                index[1] = (unsigned short)(slice == slices - 1 ? 1 : slice + 2);
                index[2] = (unsigned short)(slice + 1);
                index += 3;
            } else {
                const unsigned int previous = ringStart - slices + slice;
                if (slice == slices - 1) {
                    index[0] = (unsigned short)(ringStart - slices);
                    index[1] = (unsigned short)ringStart;
                    index[2] = (unsigned short)(ringStart - 1);
                    index[3] = (unsigned short)(ringStart - 1);
                    index[4] = (unsigned short)ringStart;
                } else {
                    index[0] = (unsigned short)(previous + 1);
                    index[1] = (unsigned short)(ringStart + slice + 1);
                    index[2] = (unsigned short)previous;
                    index[3] = (unsigned short)previous;
                    index[4] = (unsigned short)(ringStart + slice + 1);
                }
                index[5] = (unsigned short)(ringStart + slice);
                index += 6;
            }
        }
        ringStart = written;
    }

    AtVector3* bottom = Position(locked, written);
    bottom->z = (float)(D(sphere->center.z) - radius);
    bottom->y = sphere->center.y;
    bottom->x = sphere->center.x;
    const unsigned int last = ringStart - slices;
    for (unsigned int slice = 0; slice < slices; slice++) {
        index[0] = (unsigned short)written;
        index[1] = (unsigned short)(last + slice);
        index[2] = (unsigned short)(slice == slices - 1 ? last : last + slice + 1);
        index += 3;
    }
    buffer->UnlockIndices(indexCount);
    buffer->Unlock(vertices);
    buffer->Draw(this, material, 1, &instance);
}
