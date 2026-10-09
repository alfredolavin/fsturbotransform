import QtQuick
import QtQuick.Controls as QQC2

// A check box with its property's icon inside, at the left (the box and the text move right).
QQC2.CheckBox {
    id: box

    property alias iconName: icon.name

    leftPadding: 28

    PropertyIcon { id: icon }
}
