// Host test of bounding geometry through the real text tokenizer, on any pointer size. Not evidence of a byte match. The seam's four functions and the matrix table are this file's. No game data.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "at/AtBoundingGeometry.h"
#include "at/AtIoTokenizer.h"
#include "at/AtNameTableOwner.h"

static int g_fail = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); g_fail++; } } while (0)

// The other subsystems' functions: none is reached by what is tested here but the query's.
int AtSegmentQueryInsert(AtSegmentQuery* query, float key) {
    ((float*)query->m_keys)[0] = key;
    query->m_current = 0;
    return 1;
}
void AtSegmentQueryFinish(AtSegmentQuery* query, int kind) { query->m_records[0].m_unknown0C = kind; query->m_count++; }
void AtCoordSysTransformDirection(AtCoordSys*, const AtVector3* from, AtVector3* to) { *to = *from; }
int AtSphereSegment(const AtSceneSphere*, const AtVector3*, const AtVector3*, float*, const AtVector3*, AtVector3*, float*, int) { return 0; }

namespace {

class Memory : public AtIoStream {
public:
    int Refill() override { return 0; }
};

int g_heap;

const char kText[] =
    "NameSpace \"Art\"\n"
    "GroupBoundingGeometry[1]\n"
    "BoxBoundingGeometry[2]\n"
    "SphereBoundingGeometry[1]\n"
    "BoundingGeometry[4]\n"
    "{\n"
    "  GroupBoundingGeometry \"Root\"\n"
    "  {\n"
    "  }\n"
    "  BoxBoundingGeometry \"Body\"\n"
    "  {\n"
    "    Parent \"Root\"\n"
    "    RotTransCoordSys 2\n"
    "    Box 1.0 2.0 3.0 -4.0 5.0 -6.0\n"
    "  }\n"
    "  BoxBoundingGeometry \"Corners\"\n"
    "  {\n"
    "    Parent \"Root\"\n"
    "    BoxMinMax -1.0 -2.0 -3.0 3.0 4.0 5.0\n"
    "  }\n"
    "  SphereBoundingGeometry \"Ball\"\n"
    "  {\n"
    "    Parent \"Body\"\n"
    "    Sphere 1.0 -1.0 2.0 -7.5\n"
    "  }\n"
    "}\n";

void Identity(AtVector3* axes) {
    axes[0].x = 1; axes[0].y = 0; axes[0].z = 0;
    axes[1].x = 0; axes[1].y = 1; axes[1].z = 0;
    axes[2].x = 0; axes[2].y = 0; axes[2].z = 1;
}

}  // namespace

int main() {
    AtBoundingGeometryDatabase::SetHeap(&g_heap);
    AtSegmentHitList::SetHeap(&g_heap);
    GmNameMap::s_heap = &g_heap;
    GmNameSpaceDb::s_heap = &g_heap;

    AtNameTableOwner* owner = new AtNameTableOwner;
    owner->m_geometrySpaces.Init(4);

    char text[sizeof kText];
    std::memcpy(text, kText, sizeof kText);
    Memory stream;
    stream.m_base = text;
    stream.m_cur = text;
    stream.m_limit = text + sizeof(text) - 1;
    stream.m_size = sizeof(text) - 1;
    AtIoTextTokenizer tokenizer;
    tokenizer.SetStream(&stream);

    AtBoundingGeometryDatabase database;
    database.Read(owner, &tokenizer);

    CHECK(database.m_boxCount == 2);
    CHECK(database.m_sphereCount == 1);
    CHECK(database.m_groupCount == 1);
    CHECK(owner->FindBoundingGeometry("Body", "Art") == &database.m_boxes[0]);
    CHECK(owner->FindBoundingGeometry("Ball", "Art") == &database.m_spheres[0]);
    CHECK(database.Find("Root") == &database.m_groups[0]);

    AtBoxBoundingGeometry& body = database.m_boxes[0];
    CHECK(body.m_kind == AtBoundingGeometry::kBox);
    CHECK(body.m_jointIndex == 2);
    CHECK(body.m_parent == &database.m_groups[0]);
    CHECK(body.m_database == &database);
    CHECK(body.m_box.m_center.z == 3.0f);
    CHECK(body.m_box.m_half.x == 4.0f);
    CHECK(body.m_box.m_half.z == 6.0f);

    AtBoxBoundingGeometry& corners = database.m_boxes[1];
    CHECK(corners.m_box.m_half.x == 2.0f);
    CHECK(corners.m_box.m_half.y == 3.0f);
    CHECK(corners.m_box.m_half.z == 4.0f);
    CHECK(corners.m_box.m_center.x == 1.0f);
    CHECK(corners.m_box.m_center.y == 1.0f);
    CHECK(corners.m_box.m_center.z == 1.0f);
    CHECK(corners.m_nextSibling == &body);
    CHECK(database.m_groups[0].m_firstChild == &corners);

    AtSphereBoundingGeometry& ball = database.m_spheres[0];
    CHECK(ball.m_kind == AtBoundingGeometry::kSphere);
    CHECK(ball.m_sphere.radius == 7.5f);
    CHECK(ball.m_sphere.center.y == -1.0f);
    CHECK(ball.m_parent == &body);

    // Boxes: apart along one axis, then touching.
    AtVector3 axes[3];
    Identity(axes);
    AtBoundingBox a, b;
    a.m_half.x = a.m_half.y = a.m_half.z = 1;
    b.m_half.x = b.m_half.y = b.m_half.z = 1;
    AtVector3 position;
    position.x = 3; position.y = 0; position.z = 0;
    CHECK(a.Overlaps(&b, axes, &position) == 0);
    position.x = 2;
    CHECK(a.Overlaps(&b, axes, &position) == 1);
    AtVector3 normal;
    float extent, otherExtent;
    position.x = 3;
    const double depth = a.Penetration(&b, axes, &position, &normal, &extent, &otherExtent);
    CHECK(depth == 1.0);
    CHECK(normal.x == 1.0f);
    CHECK(extent == 1.0f);
    CHECK(otherExtent == 1.0f);

    // A sphere past a corner: the distance, not the sum of the axes.
    AtSceneSphere sphere;
    sphere.center.x = 2; sphere.center.y = 2; sphere.center.z = 0;
    sphere.radius = 1;
    CHECK(a.OverlapsSphere(&sphere) == 0);
    sphere.radius = 1.5f;
    CHECK(a.OverlapsSphere(&sphere) == 1);
    sphere.center.y = -2;
    sphere.radius = 1;
    const double sphereDepth = a.PenetrationSphere(&sphere, &normal, &extent, &otherExtent);
    CHECK(std::fabs(sphereDepth - (std::sqrt(2.0) - 1)) < 1e-6);
    CHECK(normal.x > 0 && normal.y < 0);

    // A segment through, and one that misses.
    AtVector3 start, end, clippedStart, clippedEnd;
    float tStart = -1, tEnd = -1;
    start.x = -3; start.y = 0; start.z = 0;
    end.x = 3; end.y = 0; end.z = 0;
    CHECK(a.Clip(&start, &end, &clippedStart, &clippedEnd, &tStart, &tEnd) == 0x48u);
    CHECK(std::fabs(clippedStart.x + 1.0f) < 1e-5f);
    CHECK(std::fabs(clippedEnd.x - 1.0f) < 1e-5f);
    CHECK(std::fabs(tStart - 1.0f / 3) < 1e-6f);
    start.y = 2; end.y = 2;
    CHECK(a.Clip(&start, &end, &clippedStart, &clippedEnd, &tStart, &tEnd) == 0u);

    // The list of hits holds what it is given, and frees what it allocated.
    AtSegmentHitList list;
    list.Allocate(3);
    CHECK(list.m_capacity == 3);
    CHECK(((unsigned char*)list.m_keys)[23] == 0);
    list.CountOne();
    list.CountOne();
    list.CountOne();
    list.CountOne();
    CHECK(list.m_count == 3);
    list.Clear();
    CHECK(list.m_keys == 0);
    CHECK(list.m_first0 == &list.m_vectors0[0]);

    // Contacts: a full list, and a list of leaves.
    AtBoundingContact items[1];
    AtBoundingContactList contacts = {1, 0, AtBoundingContactList::kLeavesOnly, items};
    contacts.Add(&database.m_groups[0], &ball, 0, 0);
    CHECK(contacts.m_count == 0);
    contacts.Add(&ball, &corners, 0, 0);
    CHECK(contacts.m_count == 1);
    CHECK(items[0].m_second == &corners);
    contacts.Add(&ball, &corners, 0, 0);
    CHECK(contacts.m_count == 1);

    // Groups say yes to everything and have no depth. Virtual dispatch on any pointer size.
    AtBoundingGeometry* group = &database.m_groups[0];
    CHECK(group->Overlaps(&ball) == 1);
    CHECK(group->TestSegment() == 1);
    CHECK(group->Penetration(0, &ball, 0, 0, 0, 0) == 0.0);

    // Clearing lets a file be read again.
    char again[sizeof kText];
    std::memcpy(again, kText, sizeof kText);
    stream.m_base = again;
    stream.m_cur = again;
    stream.m_limit = again + sizeof(again) - 1;
    AtIoTextTokenizer second;
    second.SetStream(&stream);
    database.Read(owner, &second);
    CHECK(database.m_boxCount == 2);
    CHECK(database.Find("Ball") == &database.m_spheres[0]);

    tokenizer.SetStream(0);
    second.SetStream(0);
    stream.m_base = stream.m_cur = stream.m_limit = 0;
    database.Clear();
    CHECK(database.m_boxes == 0);
    CHECK(database.m_boxCount == 0);

    if (g_fail == 0) std::printf("test_asb: ok\n");
    return g_fail != 0;
}
