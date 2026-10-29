// Linux: MPRIS (wrapper/mpris), which desktop media controls, media keys and
// playerctl use.
#include "wrapper/systemmedia/systemmedia.h"

#include "wrapper/mpris/mprisadaptor.h"

struct SystemMedia::Impl
{
    MprisAdaptor mpris;

    Impl(PlayerController *player, PlaylistController *playlist)
        : mpris(player, playlist, nullptr)
    {
    }
};

SystemMedia::SystemMedia(PlayerController *player, PlaylistController *playlist, QObject *parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>(player, playlist))
{
    connect(&m_impl->mpris, &MprisAdaptor::raiseRequested, this, &SystemMedia::raiseRequested);
    connect(&m_impl->mpris, &MprisAdaptor::quitRequested, this, &SystemMedia::quitRequested);
}

SystemMedia::~SystemMedia() = default;
