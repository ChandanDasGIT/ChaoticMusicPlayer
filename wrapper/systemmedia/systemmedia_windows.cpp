// Windows: not connected to the system's media controls yet (to come: the
// System Media Transport Controls, which media keys and the volume overlay use).
#include "wrapper/systemmedia/systemmedia.h"

struct SystemMedia::Impl
{
};

SystemMedia::SystemMedia(PlayerController *, PlaylistController *, QObject *parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>())
{
}

SystemMedia::~SystemMedia() = default;
