import QtQuick
import QtQuick.Controls

// One setting: label and what it does on the left, its control on the right, a hairline below.
Item {
    id: fieldRow
    property var field: ({})
    property string settingKey: field.key || ""
    property bool hasHint: (field.hint || "") !== ""
    property bool isFolder: field.kind === "folder"
    property real controlWidth: Math.min(330, width * 0.44)
    // A row with field.enabledBy (a toggle's key) can be changed only while that toggle is on, and
    // is greyed out otherwise; the toggle may be on another page, so it is read again on each save.
    property bool gateOn: true
    function refreshGate() { gateOn = !field.enabledBy || launcher.checked(field.enabledBy) }
    Component.onCompleted: refreshGate()
    Connections { target: launcher; function onChanged() { fieldRow.refreshGate() } }
    enabled: gateOn
    opacity: gateOn ? 1.0 : 0.45
    implicitHeight: isFolder ? 92 : Math.max(62, labels.implicitHeight + 28)
    height: implicitHeight

    Column {
        id: labels
        x: 2
        y: isFolder || hasHint ? 15 : (parent.height - height) / 2
        width: isFolder ? parent.width - 4 : parent.width - fieldRow.controlWidth - 28
        spacing: 3
        Text {
            width: parent.width; text: field.label || ""; color: Theme.text; elide: Text.ElideRight
            font.family: Theme.sans; font.pixelSize: 14
        }
        Text {
            visible: hasHint; width: parent.width; text: field.hint || ""; color: Theme.textDim
            font.family: Theme.sans; font.pixelSize: 12; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
        }
    }
    GlassSwitch {
        visible: field.kind === "toggle"
        anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
        checked: launcher.checked(settingKey)
        onToggled: launcher.setValue(settingKey, checked)
    }
    GlassComboBox {
        visible: field.kind === "choice"
        anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
        width: fieldRow.controlWidth; height: 38
        model: field.options || []
        textRole: "text"
        valueRole: "value"
        Component.onCompleted: currentIndex = indexOfValue(String(launcher.value(settingKey)))
        onActivated: launcher.setValue(settingKey, currentValue)
    }
    Row {
        visible: field.kind === "slider"
        anchors.right: parent.right; anchors.verticalCenter: parent.verticalCenter
        spacing: 10
        Slider {
            id: slider
            width: fieldRow.controlWidth - 56; height: 38
            from: field.min !== undefined ? field.min : 0
            to: field.max !== undefined ? field.max : 100
            stepSize: field.step || 1
            snapMode: Slider.SnapAlways
            Component.onCompleted: value = Number(launcher.value(settingKey)) || from
            // Saved when let go (or on a key press), not at every step of a drag: each save
            // writes settings.json and bbport.ini.
            function commit() { launcher.setValue(settingKey, Math.round(value)) }
            onPressedChanged: if (!pressed) commit()
            onMoved: if (!pressed) commit()
            background: Rectangle {
                x: slider.leftPadding; y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: slider.availableWidth; height: 4; radius: 2; color: Theme.lineStrong
                Rectangle { width: slider.visualPosition * parent.width; height: parent.height; radius: 2; color: Theme.accent }
            }
            handle: Rectangle {
                x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: 16; height: 16; radius: 8
                color: slider.pressed ? Theme.accentHover : Theme.text
            }
        }
        Text {
            width: 46; anchors.verticalCenter: parent.verticalCenter
            text: Math.round(slider.value) + (field.unit || "")
            color: Theme.text; font.family: Theme.sans; font.pixelSize: 14; horizontalAlignment: Text.AlignRight
        }
    }
    GlassTextField {
        visible: field.kind !== "toggle" && field.kind !== "choice" && field.kind !== "slider"
        x: isFolder ? 2 : parent.width - width
        y: isFolder ? 46 : (parent.height - height) / 2
        width: isFolder ? parent.width - 110 : fieldRow.controlWidth
        height: 36
        text: String(launcher.value(settingKey))
        placeholderText: isFolder ? "Default location" : ""
        echoMode: field.kind === "password" ? TextInput.Password : TextInput.Normal
        // Saved as it is typed or pasted: a page change destroys the field before it loses
        // focus, and a pasted co-op invite code was lost that way.
        onTextEdited: launcher.setValue(settingKey, text)
        onEditingFinished: launcher.setValue(settingKey, text)
    }
    GlassButton {
        visible: isFolder
        text: "Browse"
        anchors.right: parent.right; y: 46; height: 36; width: 96
        onClicked: launcher.browse(settingKey)
    }
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.line }
}
