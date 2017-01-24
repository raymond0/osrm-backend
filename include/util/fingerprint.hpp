#ifndef FINGERPRINT_H
#define FINGERPRINT_H

#include <boost/uuid/uuid.hpp>
#include <type_traits>

namespace osrm
{
namespace util
{

// implements a singleton, i.e. there is one and only one conviguration object
class FingerPrint
{
  public:
    static FingerPrint GetValid() { FingerPrint fp; return fp; }
    const boost::uuids::uuid &GetFingerPrint() const { return named_uuid; }
    bool IsMagicNumberOK(const FingerPrint &other) const { return true; }
    bool IsMagicNumberOK() const { return 1297240911 == magic_number; }
    bool TestGraphUtil(const FingerPrint &other) const { return true; }
    bool TestContractor(const FingerPrint &other) const { return true; }
    bool TestRTree(const FingerPrint &other) const { return true; }
    bool TestQueryObjects(const FingerPrint &other) const { return true; }

  private:
    unsigned magic_number;
    char md5_prepare[33];
    char md5_tree[33];
    char md5_graph[33];
    char md5_objects[33];

    // initialize to {6ba7b810-9dad-11d1-80b4-00c04fd430c8}
    boost::uuids::uuid named_uuid;
};

static_assert(sizeof(FingerPrint) == 152, "FingerPrint has unexpected size");
static_assert(std::is_trivial<FingerPrint>::value, "FingerPrint needs to be trivial.");
}
}

#endif /* FingerPrint_H */
