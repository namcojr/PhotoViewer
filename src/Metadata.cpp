#include "Metadata.h"

#include <exiv2/exiv2.hpp>

namespace {
using EasyAccessFn = Exiv2::ExifData::const_iterator (*)(const Exiv2::ExifData&);

void addEasy(Metadata::Fields& fields, const Exiv2::ExifData& exif, const char* label, EasyAccessFn fn) {
    const auto it = fn(exif);
    if (it == exif.end()) {
        return;
    }
    const QString value = QString::fromStdString(it->print(&exif)).trimmed();
    if (!value.isEmpty()) {
        fields.append({QString::fromLatin1(label), value});
    }
}

QString printKey(const Exiv2::ExifData& exif, const char* key) {
    const auto it = exif.findKey(Exiv2::ExifKey(key));
    if (it == exif.end()) {
        return {};
    }
    return QString::fromStdString(it->print(&exif)).trimmed();
}

QString gpsCoordinate(const Exiv2::ExifData& exif, const char* valueKey, const char* refKey) {
    const QString value = printKey(exif, valueKey);
    if (value.isEmpty()) {
        return {};
    }
    const QString ref = printKey(exif, refKey);
    return ref.isEmpty() ? value : QStringLiteral("%1 %2").arg(value, ref);
}
}

namespace Metadata {

void initialize() {
    Exiv2::LogMsg::setLevel(Exiv2::LogMsg::mute);
    Exiv2::XmpParser::initialize();
}

Fields readExif(const std::filesystem::path& path) {
    Fields fields;
    try {
        auto image = Exiv2::ImageFactory::open(path.string());
        if (!image) {
            return fields;
        }

        image->readMetadata();
        const auto& exif = image->exifData();
        if (exif.empty()) {
            return fields;
        }

        addEasy(fields, exif, "Make", Exiv2::make);
        addEasy(fields, exif, "Camera", Exiv2::model);
        addEasy(fields, exif, "Lens", Exiv2::lensName);
        addEasy(fields, exif, "ISO", Exiv2::isoSpeed);
        addEasy(fields, exif, "Aperture", Exiv2::fNumber);
        addEasy(fields, exif, "Shutter", Exiv2::exposureTime);
        addEasy(fields, exif, "Focal length", Exiv2::focalLength);
        addEasy(fields, exif, "Exposure bias", Exiv2::exposureBiasValue);
        addEasy(fields, exif, "Flash", Exiv2::flash);
        addEasy(fields, exif, "White balance", Exiv2::whiteBalance);
        addEasy(fields, exif, "Date taken", Exiv2::dateTimeOriginal);

        const QString lat = gpsCoordinate(exif, "Exif.GPSInfo.GPSLatitude", "Exif.GPSInfo.GPSLatitudeRef");
        const QString lon = gpsCoordinate(exif, "Exif.GPSInfo.GPSLongitude", "Exif.GPSInfo.GPSLongitudeRef");
        if (!lat.isEmpty() && !lon.isEmpty()) {
            fields.append({QStringLiteral("GPS"), QStringLiteral("%1\n%2").arg(lat, lon)});
        }

        addEasy(fields, exif, "Software", [](const Exiv2::ExifData& data) {
            return data.findKey(Exiv2::ExifKey("Exif.Image.Software"));
        });
    } catch (...) {
        // Unsupported container or corrupt metadata: show what we have (usually nothing).
    }
    return fields;
}

}
