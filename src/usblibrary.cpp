#include "usblibrary.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QStandardPaths>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>
#include <QDateTime>
#include <QVector>
#include <QStringList>
#include <QTextStream>
#include <QDebug>

namespace {

// Read a single sysfs attribute file, trimmed.
QString readAttr(const QString &dir, const QString &name)
{
    QFile f(dir + QLatin1Char('/') + name);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return QString();
    QString s = QString::fromUtf8(f.readAll());
    return s.trimmed();
}

// Read a binary sysfs file as an uppercase, space-separated hex string.
QString readHex(const QString &path, int *lenOut = nullptr)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (lenOut) *lenOut = 0;
        return QString();
    }
    const QByteArray b = f.readAll();
    if (lenOut) *lenOut = b.size();
    QString out;
    out.reserve(b.size() * 3);
    for (int i = 0; i < b.size(); ++i) {
        if (i) out += QLatin1Char(' ');
        out += QString::asprintf("%02X", static_cast<unsigned char>(b.at(i)));
    }
    return out;
}

// Human-readable USB speed from the sysfs "speed" value (Mbit/s).
QString speedText(const QString &speed)
{
    const QString s = speed.trimmed();
    if (s == QLatin1String("1.5")) return QStringLiteral("1.5 (Low-Speed)");
    if (s == QLatin1String("12")) return QStringLiteral("12 (Full-Speed)");
    if (s == QLatin1String("480")) return QStringLiteral("480 (High-Speed)");
    if (s == QLatin1String("5000")) return QStringLiteral("5000 (SuperSpeed)");
    if (s == QLatin1String("10000")) return QStringLiteral("10000 (SuperSpeed+)");
    return s.isEmpty() ? QStringLiteral("–") : s;
}

} // namespace

UsbLibrary::UsbLibrary(QObject *parent) : QObject(parent)
{
    load();
}

QString UsbLibrary::dataDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString UsbLibrary::dbPath() const
{
    return dataDir() + QStringLiteral("/usb-devices.json");
}

// --- classification --------------------------------------------------------

QString UsbLibrary::classifyKey(const QVariantMap &dev)
{
    const QVariantList intfs = dev.value(QStringLiteral("interfaces")).toList();

    bool hid = false, mouse = false, keyboard = false;
    bool storage = false, audio = false, comm = false, wireless = false,
         smartcard = false, hub = false, image = false, printer = false;

    auto note = [&](const QString &cls, const QString &proto) {
        const int c = cls.toInt(nullptr, 16);
        const int p = proto.toInt(nullptr, 16);
        switch (c) {
        case 0x03: hid = true;
            if (p == 0x02) mouse = true;
            else if (p == 0x01) keyboard = true;
            break;
        case 0x08: storage = true; break;
        case 0x01: audio = true; break;
        case 0x02: case 0x0a: comm = true; break;
        case 0x06: image = true; break;
        case 0x07: printer = true; break;
        case 0x0b: smartcard = true; break;
        case 0x09: hub = true; break;
        case 0xe0: wireless = true; break;
        default: break;
        }
    };

    for (const QVariant &v : intfs) {
        const QVariantMap m = v.toMap();
        note(m.value(QStringLiteral("class")).toString(),
             m.value(QStringLiteral("protocol")).toString());
    }
    // Fall back to the device-level class if no interfaces were parsed.
    if (intfs.isEmpty())
        note(dev.value(QStringLiteral("deviceClass")).toString(),
             dev.value(QStringLiteral("deviceProtocol")).toString());

    if (mouse) return QStringLiteral("mouse");
    if (keyboard) return QStringLiteral("keyboard");
    if (hid) return QStringLiteral("hid");
    if (storage) return QStringLiteral("storage");
    if (audio) return QStringLiteral("audio");
    if (printer) return QStringLiteral("printer");
    if (image) return QStringLiteral("image");
    if (comm) return QStringLiteral("comm");
    if (smartcard) return QStringLiteral("smartcard");
    if (wireless) return QStringLiteral("wireless");
    if (hub) return QStringLiteral("hub");
    return QStringLiteral("other");
}

int UsbLibrary::classRank(const QString &key)
{
    static const QStringList order = {
        QStringLiteral("mouse"), QStringLiteral("keyboard"), QStringLiteral("hid"),
        QStringLiteral("storage"), QStringLiteral("audio"), QStringLiteral("comm"),
        QStringLiteral("printer"), QStringLiteral("image"), QStringLiteral("smartcard"),
        QStringLiteral("wireless"), QStringLiteral("hub"), QStringLiteral("other")
    };
    const int i = order.indexOf(key);
    return i < 0 ? order.size() : i;
}

QString UsbLibrary::matchKey(const QVariantMap &dev)
{
    return dev.value(QStringLiteral("vendorId")).toString() + QLatin1Char(':')
         + dev.value(QStringLiteral("productId")).toString() + QLatin1Char(':')
         + dev.value(QStringLiteral("serial")).toString();
}

QString UsbLibrary::autoName(const QVariantMap &dev) const
{
    const QString product = dev.value(QStringLiteral("product")).toString().trimmed();
    const QString manuf = dev.value(QStringLiteral("manufacturer")).toString().trimmed();
    if (!product.isEmpty()) {
        // Prefix the maker only if it is not already part of the product name.
        if (!manuf.isEmpty() && !product.contains(manuf, Qt::CaseInsensitive))
            return manuf + QLatin1Char('-') + product;
        return product;
    }
    if (!manuf.isEmpty())
        return manuf;
    return dev.value(QStringLiteral("vendorId")).toString() + QLatin1Char(':')
         + dev.value(QStringLiteral("productId")).toString();
}

// --- usb.ids lookup + chip-vendor inference --------------------------------

void UsbLibrary::loadUsbIds() const
{
    if (m_usbIdsLoaded)
        return;
    m_usbIdsLoaded = true;   // only attempt once, even if the file is missing

    static const char *paths[] = {
        "/usr/share/hwdata/usb.ids", "/usr/share/usb.ids",
        "/var/lib/usbutils/usb.ids", "/usr/share/misc/usb.ids"
    };
    QString path;
    for (const char *p : paths) {
        if (QFile::exists(QString::fromLatin1(p))) { path = QString::fromLatin1(p); break; }
    }
    if (path.isEmpty())
        return;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QString curVid;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        if (line.startsWith(QLatin1String("\t\t")))
            continue;                                   // interface line — ignore
        if (line.startsWith(QLatin1Char('\t'))) {
            // Product line: "\tPPPP  Name"
            if (curVid.isEmpty()) continue;
            const QString body = line.mid(1);
            if (body.size() < 6) continue;
            const QString pid = body.left(4).toLower();
            bool ok = false; pid.toInt(&ok, 16);
            if (!ok) continue;
            m_usbProduct.insert(curVid + QLatin1Char(':') + pid, body.mid(6).trimmed());
        } else {
            // Vendor line: "VVVV  Name" — a hex4 at column 0.
            if (line.size() < 6) continue;
            const QString vid = line.left(4).toLower();
            bool ok = false; vid.toInt(&ok, 16);
            if (!ok) { curVid.clear(); continue; }      // e.g. class/HUT sections
            curVid = vid;
            m_usbVendor.insert(vid, line.mid(6).trimmed());
        }
    }
}

QString UsbLibrary::vendorName(const QString &vid) const
{
    loadUsbIds();
    return m_usbVendor.value(vid.toLower());
}

QString UsbLibrary::productName(const QString &vid, const QString &pid) const
{
    loadUsbIds();
    return m_usbProduct.value(vid.toLower() + QLatin1Char(':') + pid.toLower());
}

QString UsbLibrary::inferChipVendor(const QString &manufacturer, const QString &vendorName)
{
    // Known HID/mouse/keyboard silicon vendors. If any appears in the device's
    // manufacturer string or the usb.ids vendor name, it is very likely the chip
    // maker (cheap peripherals report the IC vendor there).
    static const char *chips[] = {
        "SiGma", "Holtek", "Pixart", "PixArt", "Sonix", "Microdia", "Cypress",
        "Nordic", "Realtek", "ITE", "Sunplus", "Elan", "Weltrend", "MOSART",
        "Kingsis", "Chicony", "Sunplusit", "Telink", "Zhuhai", "Sino Wealth",
        "Cortek", "Blakonik", "PixArt Imaging"
    };
    const QString hay = (manufacturer + QLatin1Char(' ') + vendorName);
    for (const char *c : chips) {
        if (hay.contains(QString::fromLatin1(c), Qt::CaseInsensitive))
            return QString::fromLatin1(c);
    }
    return QString();
}

// --- sysfs scan ------------------------------------------------------------

QVariantMap UsbLibrary::parseSysfsDevice(const QString &sysPath) const
{
    QVariantMap m;
    // --- Profile-relevant (green, sent to the Flipper) ---------------------
    m.insert(QStringLiteral("vendorId"), readAttr(sysPath, QStringLiteral("idVendor")).toLower());
    m.insert(QStringLiteral("productId"), readAttr(sysPath, QStringLiteral("idProduct")).toLower());
    m.insert(QStringLiteral("manufacturer"), readAttr(sysPath, QStringLiteral("manufacturer")));
    m.insert(QStringLiteral("product"), readAttr(sysPath, QStringLiteral("product")));

    // --- Identification via usb.ids + chip-vendor inference ---------------
    const QString vid = m.value(QStringLiteral("vendorId")).toString();
    const QString pid = m.value(QStringLiteral("productId")).toString();
    const QString vname = vendorName(vid);
    m.insert(QStringLiteral("vendorName"), vname);
    m.insert(QStringLiteral("productName"), productName(vid, pid));
    m.insert(QStringLiteral("chipVendor"),
             inferChipVendor(m.value(QStringLiteral("manufacturer")).toString(), vname));

    // --- Full device descriptor (white, informational) --------------------
    m.insert(QStringLiteral("bcdDevice"), readAttr(sysPath, QStringLiteral("bcdDevice")));
    m.insert(QStringLiteral("usbVersion"), readAttr(sysPath, QStringLiteral("version")));
    m.insert(QStringLiteral("serial"), readAttr(sysPath, QStringLiteral("serial")));
    m.insert(QStringLiteral("deviceClass"), readAttr(sysPath, QStringLiteral("bDeviceClass")));
    m.insert(QStringLiteral("deviceSubClass"), readAttr(sysPath, QStringLiteral("bDeviceSubClass")));
    m.insert(QStringLiteral("deviceProtocol"), readAttr(sysPath, QStringLiteral("bDeviceProtocol")));
    m.insert(QStringLiteral("maxPacketSize0"), readAttr(sysPath, QStringLiteral("bMaxPacketSize0")));
    m.insert(QStringLiteral("numConfigurations"), readAttr(sysPath, QStringLiteral("bNumConfigurations")));
    m.insert(QStringLiteral("numInterfaces"), readAttr(sysPath, QStringLiteral("bNumInterfaces")).trimmed());
    m.insert(QStringLiteral("configValue"), readAttr(sysPath, QStringLiteral("bConfigurationValue")));
    m.insert(QStringLiteral("attributes"), readAttr(sysPath, QStringLiteral("bmAttributes")));
    m.insert(QStringLiteral("maxPower"), readAttr(sysPath, QStringLiteral("bMaxPower")));
    m.insert(QStringLiteral("speed"), readAttr(sysPath, QStringLiteral("speed")));
    m.insert(QStringLiteral("speedText"), speedText(readAttr(sysPath, QStringLiteral("speed"))));
    m.insert(QStringLiteral("busnum"), readAttr(sysPath, QStringLiteral("busnum")));
    m.insert(QStringLiteral("devnum"), readAttr(sysPath, QStringLiteral("devnum")));
    m.insert(QStringLiteral("removable"), readAttr(sysPath, QStringLiteral("removable")));

    // Raw device+config descriptor blob (exactly what lsusb parses).
    int rawLen = 0;
    m.insert(QStringLiteral("rawDescriptors"),
             readHex(sysPath + QStringLiteral("/descriptors"), &rawLen));
    m.insert(QStringLiteral("rawDescriptorsLen"), rawLen);

    // --- Interfaces, endpoints, HID report descriptor ---------------------
    const QString devName = QFileInfo(sysPath).fileName();
    QVariantList intfs;
    QString reportDesc;
    int reportDescLen = 0;
    QDir dir(sysPath);
    const QStringList subs = dir.entryList(QStringList() << (devName + QStringLiteral(":*")),
                                           QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &sub : subs) {
        const QString ipath = sysPath + QLatin1Char('/') + sub;
        const QString icls = readAttr(ipath, QStringLiteral("bInterfaceClass"));
        if (icls.isEmpty())
            continue;
        QVariantMap im;
        im.insert(QStringLiteral("class"), icls);
        im.insert(QStringLiteral("subclass"), readAttr(ipath, QStringLiteral("bInterfaceSubClass")));
        im.insert(QStringLiteral("protocol"), readAttr(ipath, QStringLiteral("bInterfaceProtocol")));
        im.insert(QStringLiteral("number"), readAttr(ipath, QStringLiteral("bInterfaceNumber")).trimmed());
        im.insert(QStringLiteral("altSetting"), readAttr(ipath, QStringLiteral("bAlternateSetting")).trimmed());
        im.insert(QStringLiteral("numEndpoints"), readAttr(ipath, QStringLiteral("bNumEndpoints")).trimmed());
        // Bound kernel driver (e.g. usbhid), via the "driver" symlink.
        const QString drv = QFileInfo(ipath + QStringLiteral("/driver")).symLinkTarget();
        im.insert(QStringLiteral("driver"), drv.isEmpty() ? QString() : QFileInfo(drv).fileName());

        // Endpoints: ep_XX subdirectories.
        QVariantList eps;
        QDir idir(ipath);
        const QStringList epDirs = idir.entryList(QStringList() << QStringLiteral("ep_*"),
                                                  QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &ep : epDirs) {
            const QString eppath = ipath + QLatin1Char('/') + ep;
            QVariantMap em;
            em.insert(QStringLiteral("address"), readAttr(eppath, QStringLiteral("bEndpointAddress")));
            em.insert(QStringLiteral("type"), readAttr(eppath, QStringLiteral("type")));
            em.insert(QStringLiteral("maxPacket"), readAttr(eppath, QStringLiteral("wMaxPacketSize")));
            em.insert(QStringLiteral("interval"), readAttr(eppath, QStringLiteral("interval")));
            eps.append(em);
        }
        im.insert(QStringLiteral("endpoints"), eps);

        // HID report descriptor lives in a HID class subdir of the interface.
        if (reportDesc.isEmpty()) {
            const QStringList hidDirs = idir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
            for (const QString &hd : hidDirs) {
                const QString rdpath = ipath + QLatin1Char('/') + hd + QStringLiteral("/report_descriptor");
                if (QFile::exists(rdpath)) {
                    reportDesc = readHex(rdpath, &reportDescLen);
                    break;
                }
            }
        }
        intfs.append(im);
    }
    m.insert(QStringLiteral("interfaces"), intfs);
    m.insert(QStringLiteral("reportDescriptor"), reportDesc);
    m.insert(QStringLiteral("reportDescriptorLen"), reportDescLen);

    // --- Host controller (the phone's USB chip serving this device) --------
    // Derive it from the canonical device path, e.g.
    //   /sys/devices/platform/soc/a600000.ssusb/a600000.dwc3/xhci-hcd.0.auto/usb1/1-1
    const QString canon = QFileInfo(sysPath).canonicalFilePath();
    QString controller, hcd;
    const QStringList parts = canon.split(QLatin1Char('/'), QString::SkipEmptyParts);
    for (const QString &p : parts) {
        if (p.contains(QLatin1String("dwc3")) || p.contains(QLatin1String("ssusb")))
            controller = p;
        if (p.contains(QLatin1String("hcd")) || p.contains(QLatin1String("xhci")))
            hcd = p;
    }
    QString host = controller;
    if (!hcd.isEmpty())
        host += (host.isEmpty() ? QString() : QStringLiteral(" · ")) + hcd;
    m.insert(QStringLiteral("hostController"), host);

    m.insert(QStringLiteral("classKey"), classifyKey(m));
    return m;
}

QVariantList UsbLibrary::scanAttached() const
{
    QVariantList result;
    QDir base(QStringLiteral("/sys/bus/usb/devices"));
    if (!base.exists()) {
        qWarning() << "UsbLibrary: /sys/bus/usb/devices missing";
        return result;
    }
    // The entries here are symlinks (e.g. 1-1 -> ../../devices/.../1-1). Do NOT
    // filter by QDir::Dirs — Qt may not classify a symlink-to-dir as a dir on
    // sysfs. List everything and probe for idVendor instead.
    const QStringList entries = base.entryList(
        QDir::NoDotAndDotDot | QDir::System | QDir::Hidden | QDir::AllEntries, QDir::Name);
    qWarning() << "UsbLibrary: scan entries" << entries;
    for (const QString &name : entries) {
        // Skip root hubs (usb1, usb2, ...) and interface symlinks (contain ':').
        if (name.startsWith(QLatin1String("usb")) || name.contains(QLatin1Char(':')))
            continue;
        const QString sysPath = base.absoluteFilePath(name);
        if (!QFile::exists(sysPath + QStringLiteral("/idVendor")))
            continue;
        QVariantMap dev = parseSysfsDevice(sysPath);
        if (dev.value(QStringLiteral("vendorId")).toString().isEmpty())
            continue;
        // Skip plain hubs — they are just cabling, not identities to clone.
        if (dev.value(QStringLiteral("classKey")).toString() == QLatin1String("hub"))
            continue;

        const QString mk = matchKey(dev);
        bool known = false;
        for (const QVariantMap &saved : m_db) {
            if (matchKey(saved) == mk) { known = true; break; }
        }
        dev.insert(QStringLiteral("known"), known);
        dev.insert(QStringLiteral("suggestedName"), autoName(dev));
        dev.insert(QStringLiteral("sysPath"), sysPath);
        result.append(dev);
        qWarning() << "UsbLibrary: found" << name
                   << dev.value(QStringLiteral("vendorId")).toString()
                   << dev.value(QStringLiteral("productId")).toString()
                   << dev.value(QStringLiteral("classKey")).toString();
    }
    qWarning() << "UsbLibrary: scan produced" << result.size() << "device(s)";
    return result;
}

// --- library CRUD ----------------------------------------------------------

QVariantList UsbLibrary::devices() const
{
    QList<QVariantMap> sorted = m_db;
    std::sort(sorted.begin(), sorted.end(), [](const QVariantMap &a, const QVariantMap &b) {
        const int ra = classRank(a.value(QStringLiteral("classKey")).toString());
        const int rb = classRank(b.value(QStringLiteral("classKey")).toString());
        if (ra != rb) return ra < rb;
        return a.value(QStringLiteral("name")).toString().localeAwareCompare(
                   b.value(QStringLiteral("name")).toString()) < 0;
    });
    QVariantList out;
    for (const QVariantMap &m : sorted)
        out.append(m);
    return out;
}

// --- HID report descriptor decoder -----------------------------------------

namespace {

QString hidUsagePageName(quint32 p)
{
    switch (p) {
    case 0x01: return QStringLiteral("Generic Desktop");
    case 0x02: return QStringLiteral("Simulation");
    case 0x03: return QStringLiteral("VR");
    case 0x04: return QStringLiteral("Sport");
    case 0x05: return QStringLiteral("Game");
    case 0x06: return QStringLiteral("Generic Device");
    case 0x07: return QStringLiteral("Keyboard/Keypad");
    case 0x08: return QStringLiteral("LEDs");
    case 0x09: return QStringLiteral("Button");
    case 0x0A: return QStringLiteral("Ordinal");
    case 0x0B: return QStringLiteral("Telephony");
    case 0x0C: return QStringLiteral("Consumer");
    case 0x0D: return QStringLiteral("Digitizer");
    case 0x0F: return QStringLiteral("PID (force feedback)");
    case 0x14: return QStringLiteral("Alphanumeric Display");
    case 0xFF00: return QStringLiteral("Vendor-defined");
    default:
        if (p >= 0xFF00) return QStringLiteral("Vendor-defined");
        return QString::asprintf("0x%02X", p);
    }
}

// Usages within the Generic Desktop page (the interesting ones for pointers).
QString hidDesktopUsage(quint32 u)
{
    switch (u) {
    case 0x01: return QStringLiteral("Pointer");
    case 0x02: return QStringLiteral("Mouse");
    case 0x04: return QStringLiteral("Joystick");
    case 0x05: return QStringLiteral("Gamepad");
    case 0x06: return QStringLiteral("Keyboard");
    case 0x07: return QStringLiteral("Keypad");
    case 0x30: return QStringLiteral("X");
    case 0x31: return QStringLiteral("Y");
    case 0x32: return QStringLiteral("Z");
    case 0x33: return QStringLiteral("Rx");
    case 0x34: return QStringLiteral("Ry");
    case 0x35: return QStringLiteral("Rz");
    case 0x36: return QStringLiteral("Slider");
    case 0x37: return QStringLiteral("Dial");
    case 0x38: return QStringLiteral("Wheel");
    case 0x39: return QStringLiteral("Hat Switch");
    case 0x3C: return QStringLiteral("Motion Wakeup");
    case 0x80: return QStringLiteral("System Control");
    default: return QString::asprintf("0x%02X", u);
    }
}

QString hidMainFlags(quint32 v)
{
    QStringList f;
    f << ((v & 0x01) ? QStringLiteral("Const") : QStringLiteral("Data"));
    f << ((v & 0x02) ? QStringLiteral("Var") : QStringLiteral("Array"));
    f << ((v & 0x04) ? QStringLiteral("Rel") : QStringLiteral("Abs"));
    if (v & 0x08) f << QStringLiteral("Wrap");
    if (v & 0x10) f << QStringLiteral("NonLinear");
    if (v & 0x20) f << QStringLiteral("NoPreferred");
    if (v & 0x40) f << QStringLiteral("Null");
    if (v & 0x80) f << QStringLiteral("Volatile");
    return f.join(QLatin1Char(','));
}

QString hidCollection(quint32 v)
{
    switch (v) {
    case 0x00: return QStringLiteral("Physical");
    case 0x01: return QStringLiteral("Application");
    case 0x02: return QStringLiteral("Logical");
    case 0x03: return QStringLiteral("Report");
    case 0x04: return QStringLiteral("Named Array");
    default: return QString::asprintf("0x%02X", v);
    }
}

} // namespace

QString UsbLibrary::decodeHidReport(const QString &hex) const
{
    if (hex.trimmed().isEmpty())
        return QString();

    // Parse the space-separated hex into bytes.
    QVector<quint8> b;
    const QStringList toks = hex.split(QLatin1Char(' '), QString::SkipEmptyParts);
    for (const QString &t : toks) {
        bool ok = false;
        const uint v = t.toUInt(&ok, 16);
        if (ok) b.append(quint8(v & 0xFF));
    }

    QString out;
    int indent = 0;
    quint32 curPage = 0;

    auto pad = [&indent]() { return QString(indent * 2, QLatin1Char(' ')); };

    for (int i = 0; i < b.size();) {
        const int itemStart = i;      // index of the prefix byte
        const quint8 prefix = b[i++];
        if (prefix == 0xFE) { // long item — skip
            if (i >= b.size()) break;
            const quint8 dsize = b[i++];
            if (i < b.size()) i++;       // bTag
            i += dsize;
            continue;
        }
        const int bSize = prefix & 0x03;
        const int nbytes = (bSize == 3) ? 4 : bSize;
        const int bType = (prefix >> 2) & 0x03;
        const int bTag = (prefix >> 4) & 0x0F;

        quint32 val = 0;
        for (int k = 0; k < nbytes && i < b.size(); ++k)
            val |= quint32(b[i++]) << (8 * k);
        // Signed interpretation for min/max fields.
        qint32 sval = qint32(val);
        if (nbytes == 1 && (val & 0x80)) sval = qint32(val) - 0x100;
        else if (nbytes == 2 && (val & 0x8000)) sval = qint32(val) - 0x10000;

        // Raw bytes of this item, for visual mapping decode <-> hex.
        QString itemHex;
        for (int k = itemStart; k < i; ++k) {
            if (!itemHex.isEmpty()) itemHex += QLatin1Char(' ');
            itemHex += QString::asprintf("%02X", b[k]);
        }

        QString line;
        if (bType == 0) {                 // Main
            switch (bTag) {
            case 0x8: line = QStringLiteral("Input (%1)").arg(hidMainFlags(val)); break;
            case 0x9: line = QStringLiteral("Output (%1)").arg(hidMainFlags(val)); break;
            case 0xB: line = QStringLiteral("Feature (%1)").arg(hidMainFlags(val)); break;
            case 0xA:
                out += pad() + QStringLiteral("Collection (%1)  [%2]\n")
                                   .arg(hidCollection(val)).arg(itemHex);
                indent++;
                continue;
            case 0xC:
                if (indent > 0) indent--;
                out += pad() + QStringLiteral("End Collection  [%1]\n").arg(itemHex);
                continue;
            default: line = QStringLiteral("Main 0x%1 (0x%2)").arg(bTag, 0, 16).arg(val, 0, 16); break;
            }
        } else if (bType == 1) {          // Global
            switch (bTag) {
            case 0x0: curPage = val; line = QStringLiteral("Usage Page (%1)").arg(hidUsagePageName(val)); break;
            case 0x1: line = QStringLiteral("Logical Minimum (%1)").arg(sval); break;
            case 0x2: line = QStringLiteral("Logical Maximum (%1)").arg(sval); break;
            case 0x3: line = QStringLiteral("Physical Minimum (%1)").arg(sval); break;
            case 0x4: line = QStringLiteral("Physical Maximum (%1)").arg(sval); break;
            case 0x5: line = QStringLiteral("Unit Exponent (%1)").arg(sval); break;
            case 0x6: line = QStringLiteral("Unit (0x%1)").arg(val, 0, 16); break;
            case 0x7: line = QStringLiteral("Report Size (%1)").arg(val); break;
            case 0x8: line = QStringLiteral("Report ID (%1)").arg(val); break;
            case 0x9: line = QStringLiteral("Report Count (%1)").arg(val); break;
            case 0xA: line = QStringLiteral("Push"); break;
            case 0xB: line = QStringLiteral("Pop"); break;
            default: line = QStringLiteral("Global 0x%1 (0x%2)").arg(bTag, 0, 16).arg(val, 0, 16); break;
            }
        } else if (bType == 2) {          // Local
            auto usageName = [&](quint32 u) {
                if (curPage == 0x01) return hidDesktopUsage(u);
                if (curPage == 0x09) return QStringLiteral("Button %1").arg(u);
                return QString::asprintf("0x%02X", u);
            };
            switch (bTag) {
            case 0x0: line = QStringLiteral("Usage (%1)").arg(usageName(val)); break;
            case 0x1: line = QStringLiteral("Usage Minimum (%1)").arg(usageName(val)); break;
            case 0x2: line = QStringLiteral("Usage Maximum (%1)").arg(usageName(val)); break;
            case 0x7: line = QStringLiteral("String Index (%1)").arg(val); break;
            case 0x8: line = QStringLiteral("String Minimum (%1)").arg(val); break;
            case 0x9: line = QStringLiteral("String Maximum (%1)").arg(val); break;
            case 0xA: line = QStringLiteral("Delimiter (%1)").arg(val); break;
            default: line = QStringLiteral("Local 0x%1 (0x%2)").arg(bTag, 0, 16).arg(val, 0, 16); break;
            }
        } else {
            line = QStringLiteral("? 0x%1").arg(prefix, 0, 16);
        }
        out += pad() + line + QStringLiteral("  [%1]").arg(itemHex) + QLatin1Char('\n');
    }
    return out.trimmed();
}

QVariantMap UsbLibrary::device(const QString &id) const
{
    for (const QVariantMap &m : m_db)
        if (m.value(QStringLiteral("id")).toString() == id)
            return m;
    return QVariantMap();
}

QString UsbLibrary::addDevice(const QVariantMap &dev)
{
    QVariantMap entry = dev;
    // Strip scan-only helper keys.
    entry.remove(QStringLiteral("known"));
    entry.remove(QStringLiteral("suggestedName"));
    entry.remove(QStringLiteral("sysPath"));

    // toString() yields "{...}" on this Qt; strip the braces for a clean id.
    QString id = QUuid::createUuid().toString();
    id.remove(QLatin1Char('{')).remove(QLatin1Char('}'));
    entry.insert(QStringLiteral("id"), id);

    QString name = entry.value(QStringLiteral("name")).toString().trimmed();
    if (name.isEmpty())
        name = autoName(entry);
    entry.insert(QStringLiteral("name"), name);

    if (!entry.contains(QStringLiteral("classKey")))
        entry.insert(QStringLiteral("classKey"), classifyKey(entry));
    entry.insert(QStringLiteral("savedAt"),
                 QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

    m_db.append(entry);
    save();
    emit libraryChanged();
    return id;
}

void UsbLibrary::renameDevice(const QString &id, const QString &name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return;
    for (int i = 0; i < m_db.size(); ++i) {
        if (m_db[i].value(QStringLiteral("id")).toString() == id) {
            m_db[i].insert(QStringLiteral("name"), trimmed);
            save();
            emit libraryChanged();
            return;
        }
    }
}

void UsbLibrary::removeDevice(const QString &id)
{
    for (int i = 0; i < m_db.size(); ++i) {
        if (m_db[i].value(QStringLiteral("id")).toString() == id) {
            m_db.removeAt(i);
            save();
            emit libraryChanged();
            return;
        }
    }
}

// --- persistence -----------------------------------------------------------

void UsbLibrary::load()
{
    m_db.clear();
    QFile f(dbPath());
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    if (!doc.isArray())
        return;
    const QJsonArray arr = doc.array();
    for (const QJsonValue &v : arr) {
        if (v.isObject())
            m_db.append(v.toObject().toVariantMap());
    }
}

void UsbLibrary::save() const
{
    QDir().mkpath(dataDir());
    QJsonArray arr;
    for (const QVariantMap &m : m_db)
        arr.append(QJsonObject::fromVariantMap(m));
    QFile f(dbPath());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "UsbLibrary: cannot write" << dbPath();
        return;
    }
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}
