import QtQuick
import QtQuick.Controls as QQC2

// A text field with its property's icon inside, at the left.
QQC2.TextField {
    id: field

    property alias iconName: icon.name
    property alias iconEmblem: icon.emblem

    leftPadding: 28
    Accessible.name: placeholderText

    PropertyIcon { id: icon }
}
