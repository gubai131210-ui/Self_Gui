#include "ui/theme/uistyle.h"

namespace Selt {

UiStyle::Palette &UiStyle::paletteRef()
{
    static Palette palette = Palette::Light;
    return palette;
}

void UiStyle::setActivePalette(Palette palette)
{
    paletteRef() = palette;
}

UiStyle::Palette UiStyle::activePalette()
{
    return paletteRef();
}

} // namespace Selt
