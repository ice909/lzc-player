import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts
import QtQml

import mpvtest 1.0

Item {
    width: 1280
    height: 720
    readonly property var hostWindow: Window.window
    property bool controlsVisible: true
    readonly property var playbackSpeedOptions: [
        { value: 1.75, label: "1.75x" },
        { value: 2.0, label: "2.0x" },
        { value: 1.5, label: "1.5x" },
        { value: 1.25, label: "1.25x" },
        { value: 1.0, label: "1.0x" },
        { value: 0.7, label: "0.7x" },
        { value: 0.5, label: "0.5x" }
    ]
    readonly property bool speedPanelVisible:
        speedButtonHoverHandler.hovered || speedGapMouseArea.containsMouse || speedPanelHoverHandler.hovered
    readonly property real speedButtonReservedWidth: Math.ceil(Math.max(speedDefaultTextMetrics.advanceWidth, speedMaxTextMetrics.advanceWidth)) + 16
    readonly property bool overlayOpen:
        speedPanelVisible || qualityMenu.opened || subtitleMenu.opened || volumePopup.opened

    function syncControlsVisibility() {
        if (!renderer.playing || overlayOpen) {
            controlsVisible = true
            hideControlsTimer.stop()
            return
        }

        hideControlsTimer.restart()
    }

    function showControlsTemporarily() {
        controlsVisible = true
        syncControlsVisibility()
    }

    function formatTime(seconds) {
        const safe = Math.max(0, Math.floor(seconds || 0))
        const hours = Math.floor(safe / 3600)
        const minutes = Math.floor((safe % 3600) / 60)
        const secs = safe % 60

        function pad(value) {
            return value < 10 ? "0" + value : String(value)
        }

        if (hours > 0) {
            return hours + ":" + pad(minutes) + ":" + pad(secs)
        }
        return pad(minutes) + ":" + pad(secs)
    }

    function formatNetworkSpeed(bytesPerSecond) {
        const speed = Math.max(0, Number(bytesPerSecond) || 0)
        if (speed >= 1024 * 1024) {
            return (speed / (1024 * 1024)).toFixed(2) + " MB/s"
        }
        if (speed >= 1024) {
            return (speed / 1024).toFixed(1) + " KB/s"
        }
        return Math.round(speed) + " B/s"
    }

    function playbackSpeedMatches(value) {
        return Math.abs((Number(renderer.playbackSpeed) || 1.0) - value) < 0.001
    }

    function formatPlaybackSpeedLabel(value) {
        const speed = Number(value) || 1.0
        for (let i = 0; i < playbackSpeedOptions.length; ++i) {
            const option = playbackSpeedOptions[i]
            if (Math.abs(option.value - speed) < 0.001) {
                return option.label
            }
        }

        const scaled = Math.round(speed * 100)
        return (scaled % 10 === 0 ? speed.toFixed(1) : speed.toFixed(2)) + "x"
    }

    function playbackSpeedButtonText() {
        return playbackSpeedMatches(1.0) ? "倍速" : formatPlaybackSpeedLabel(renderer.playbackSpeed)
    }

    TextMetrics {
        id: speedDefaultTextMetrics
        font.pixelSize: 14
        font.weight: Font.Medium
        text: "倍速"
    }

    TextMetrics {
        id: speedMaxTextMetrics
        font.pixelSize: 14
        font.weight: Font.Medium
        text: "1.75x"
    }

    component ControlButton: Button {
        id: controlButton
        property url iconSource: ""
        property int iconSize: 20
        property bool chromeless: false
        readonly property bool iconOnly: iconSource.toString().length > 0

        implicitHeight: iconOnly ? iconSize : 40
        implicitWidth: iconOnly ? iconSize : Math.max(40, contentItem.implicitWidth + 18)
        padding: 0

        background: Rectangle {
            radius: controlButton.chromeless ? 0 : 12
            color: controlButton.chromeless
                ? "transparent"
                : parent.down ? "#3A3F55" : parent.hovered ? "#31364A" : "#262B3D"
            border.width: controlButton.chromeless ? 0 : 1
            border.color: controlButton.chromeless
                ? "transparent"
                : parent.highlighted ? "#8FB2FF" : "#3C435D"
        }

        contentItem: Item {
            implicitWidth: controlButton.iconOnly ? icon.implicitWidth : label.implicitWidth
            implicitHeight: controlButton.iconOnly ? icon.implicitHeight : label.implicitHeight

            Image {
                id: icon
                anchors.centerIn: parent
                visible: controlButton.iconOnly
                source: controlButton.iconSource
                sourceSize.width: controlButton.iconSize
                sourceSize.height: controlButton.iconSize
                width: controlButton.iconSize
                height: controlButton.iconSize
                fillMode: Image.PreserveAspectFit
                smooth: true
                mipmap: true
            }

            Text {
                id: label
                anchors.fill: parent
                visible: !controlButton.iconOnly
                text: controlButton.text
                color: "#F3F6FF"
                font.pixelSize: 14
                font.weight: Font.Medium
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
        }
    }

    MpvObject {
        id: renderer
        objectName: "renderer"
        anchors.fill: parent

        Component.onCompleted: {
            if (initialFile) {
                renderer.loadFile(initialFile)
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#000000"
        z: -1
    }

    Rectangle {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 18
        anchors.rightMargin: 18
        radius: 14
        color: "#B8141924"
        border.width: 1
        border.color: "#39445F"
        z: 2

        implicitWidth: speedInfoRow.implicitWidth + 24
        implicitHeight: speedInfoRow.implicitHeight + 16

        Row {
            id: speedInfoRow
            anchors.centerIn: parent
            spacing: 8

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 8
                height: 8
                radius: 4
                color: renderer.networkSpeed > 0 ? "#7EA8FF" : "#5A657F"
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: "网速 " + formatNetworkSpeed(renderer.networkSpeed)
                color: "#F3F6FF"
                font.pixelSize: 13
                font.weight: Font.Medium
            }
        }
    }

    HoverHandler {
        onHoveredChanged: {
            if (hovered) {
                showControlsTemporarily()
            }
        }

        onPointChanged: showControlsTemporarily()
    }

    Timer {
        id: hideControlsTimer
        interval: 1800
        repeat: false
        onTriggered: {
            if (renderer.playing && !overlayOpen) {
                controlsVisible = false
            }
        }
    }

    Connections {
        target: renderer

        function onPlayingChanged() {
            syncControlsVisibility()
        }
    }

    onOverlayOpenChanged: syncControlsVisibility()

    Shortcut {
        sequence: "Space"
        context: Qt.ApplicationShortcut
        onActivated: renderer.togglePause()
    }

    Shortcut {
        sequence: "I"
        context: Qt.ApplicationShortcut
        onActivated: renderer.command([
            "script-binding",
            "stats/display-stats-toggle"
        ])
    }

    Shortcut {
        sequence: "`"
        context: Qt.ApplicationShortcut
        onActivated: renderer.command([
            "script-binding",
            "commands/open"
        ])
    }

    Shortcut {
        sequence: "Esc"
        context: Qt.ApplicationShortcut
        onActivated: {
            if (renderer.consoleOpen) {
                renderer.command([
                    "keypress",
                    "ESC"
                ])
            }
        }
    }

    Keys.onPressed: (event) => {
        if (!renderer.consoleOpen) {
            return
        }

        if (event.key === Qt.Key_Escape) {
            event.accepted = true
            return
        }

        if (event.key === Qt.Key_Backspace) {
            renderer.command(["keypress", "BS"])
            event.accepted = true
            return
        }

        if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
            renderer.command(["keypress", "ENTER"])
            event.accepted = true
            return
        }

        if (event.key === Qt.Key_Left) {
            renderer.command(["keypress", "LEFT"])
            event.accepted = true
            return
        }

        if (event.key === Qt.Key_Right) {
            renderer.command(["keypress", "RIGHT"])
            event.accepted = true
            return
        }

        if (event.key === Qt.Key_Up) {
            renderer.command(["keypress", "UP"])
            event.accepted = true
            return
        }

        if (event.key === Qt.Key_Down) {
            renderer.command(["keypress", "DOWN"])
            event.accepted = true
            return
        }

        if (event.text && event.text.length > 0 && event.text >= " ") {
            renderer.command(["keypress", event.text])
            event.accepted = true
        }
    }

    Rectangle {
        id: controlsBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 48
        y: controlsVisible ? parent.height - height : parent.height + 10
        color: "transparent"
        border.width: 0
        opacity: controlsVisible ? 1 : 0
        enabled: controlsVisible

        Behavior on opacity {
            NumberAnimation {
                duration: 180
                easing.type: Easing.OutCubic
            }
        }

        Behavior on y {
            NumberAnimation {
                duration: 220
                easing.type: Easing.OutCubic
            }
        }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#00000000" }
                GradientStop { position: 0.33; color: "#5E000000" }
                GradientStop { position: 0.66; color: "#BF000000" }
                GradientStop { position: 1.0; color: "#BF000000" }
            }
        }

        Item {
            id: progressBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            anchors.bottom: parent.top
            Layout.preferredHeight: 24
            height: 24
            property bool dragging: false
            property bool awaitingSeek: false
            property real previewValue: 0
            readonly property real maxValue: Math.max(renderer.duration, 0.001)
            readonly property real shownValue: dragging || awaitingSeek ? previewValue : renderer.timePos
            readonly property real progress: Math.max(0, Math.min(1, shownValue / maxValue))

            function updateFromPosition(positionX) {
                const ratio = Math.max(0, Math.min(1, positionX / width))
                previewValue = ratio * maxValue
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                height: 6
                radius: 3
                color: "#253047"

                Rectangle {
                    width: parent.width * progressBar.progress
                    height: parent.height
                    radius: parent.radius
                    color: "#7EA8FF"
                }
            }

            Rectangle {
                x: progressBar.progress * (progressBar.width - width)
                anchors.verticalCenter: parent.verticalCenter
                width: 14
                height: 14
                radius: 7
                color: "#F5F8FF"
                border.width: 2
                border.color: "#6B96FF"
            }

            MouseArea {
                anchors.fill: parent

                onPressed: (mouse) => {
                    progressBar.dragging = true
                    progressBar.updateFromPosition(mouse.x)
                }

                onPositionChanged: (mouse) => {
                    if (pressed) {
                        progressBar.updateFromPosition(mouse.x)
                    }
                }

                onReleased: (mouse) => {
                    progressBar.updateFromPosition(mouse.x)
                    progressBar.awaitingSeek = true
                    renderer.seekTo(progressBar.previewValue)
                    progressBar.dragging = false
                }

                onCanceled: {
                    progressBar.dragging = false
                    progressBar.awaitingSeek = false
                }
            }

            Connections {
                target: renderer

                function onTimePosChanged() {
                    if (
                        progressBar.awaitingSeek
                        && Math.abs(renderer.timePos - progressBar.previewValue) <= 0.5
                    ) {
                        progressBar.awaitingSeek = false
                    }
                }
            }
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            spacing: 12

            RowLayout {
                spacing: 16
                Layout.alignment: Qt.AlignVCenter

                ControlButton {
                    iconSource: renderer.playing
                        ? "qrc:/lzc-player/assets/pause.svg"
                        : "qrc:/lzc-player/assets/play.svg"
                    iconSize: 24
                    chromeless: true
                    onClicked: renderer.togglePause()
                }

                ControlButton {
                    iconSource: "qrc:/lzc-player/assets/seek-backward.svg"
                    iconSize: 20
                    chromeless: true
                    onClicked: renderer.seekRelative(-10)
                }

                ControlButton {
                    iconSource: "qrc:/lzc-player/assets/seek-forward.svg"
                    iconSize: 20
                    chromeless: true
                    onClicked: renderer.seekRelative(10)
                }

                Text {
                    Layout.alignment: Qt.AlignVCenter
                    textFormat: Text.RichText
                    color: "#FFFFFF"
                    font.pixelSize: 14
                    font.weight: Font.Medium
                    text: "<span style='color:#FFFFFF;'>"
                        + formatTime(progressBar.dragging ? progressBar.previewValue : renderer.timePos)
                        + " / </span><span style='color:rgba(255,255,255,0.8);'>"
                        + formatTime(renderer.duration)
                        + "</span>"
                }
            }

            Item {
                Layout.fillWidth: true
            }

            RowLayout {
                spacing: 8
                Layout.alignment: Qt.AlignVCenter

                Item {
                    id: speedSelector
                    Layout.alignment: Qt.AlignVCenter
                    implicitWidth: Math.max(speedButtonReservedWidth, speedPanel.width)
                    implicitHeight: Math.max(24, speedButtonLabel.implicitHeight)

                    Rectangle {
                        id: speedPanel
                        visible: speedPanelVisible
                        anchors.horizontalCenter: speedTrigger.horizontalCenter
                        anchors.bottom: speedTrigger.top
                        anchors.bottomMargin: 8
                        width: 96
                        height: speedOptionsColumn.implicitHeight + 16
                        radius: 10
                        color: "#141924"
                        border.width: 1
                        border.color: Qt.rgba(1, 1, 1, 0.15)
                        z: 2

                        HoverHandler {
                            id: speedPanelHoverHandler
                        }

                        Column {
                            id: speedOptionsColumn
                            x: 8
                            y: 8
                            width: parent.width - 16
                            spacing: 4

                            Repeater {
                                model: playbackSpeedOptions

                                delegate: Rectangle {
                                    required property var modelData
                                    readonly property bool selected: playbackSpeedMatches(modelData.value)

                                    width: parent ? parent.width : 80
                                    height: 32
                                    implicitWidth: 80
                                    implicitHeight: 32
                                    radius: 6
                                    color: speedOptionMouseArea.containsMouse ? Qt.rgba(1, 1, 1, 0.04) : "transparent"

                                    Text {
                                        anchors.left: parent.left
                                        anchors.leftMargin: 10
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.label
                                        color: parent.selected ? Qt.rgba(33 / 255, 115 / 255, 223 / 255, 1) : "#CBD5E0"
                                        font.pixelSize: 14
                                        font.weight: Font.Medium
                                    }

                                    Text {
                                        anchors.right: parent.right
                                        anchors.rightMargin: 10
                                        anchors.verticalCenter: parent.verticalCenter
                                        visible: parent.selected
                                        text: "✓"
                                        color: "#FFFFFF"
                                        font.pixelSize: 14
                                        font.weight: Font.DemiBold
                                    }

                                    MouseArea {
                                        id: speedOptionMouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: renderer.setPlaybackSpeed(parent.modelData.value)
                                    }
                                }
                            }
                        }
                    }

                    MouseArea {
                        id: speedGapMouseArea
                        anchors.right: speedPanel.right
                        anchors.bottom: speedTrigger.top
                        width: speedPanel.width
                        height: speedPanel.anchors.bottomMargin
                        hoverEnabled: true
                        acceptedButtons: Qt.NoButton
                    }

                    Item {
                        id: speedTrigger
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: speedButtonReservedWidth
                        height: Math.max(24, speedButtonLabel.implicitHeight)

                        HoverHandler {
                            id: speedButtonHoverHandler
                            cursorShape: Qt.PointingHandCursor
                        }

                        Text {
                            id: speedButtonLabel
                            anchors.centerIn: parent
                            text: playbackSpeedButtonText()
                            color: "#CBD5E0"
                            font.pixelSize: 14
                            font.weight: Font.Medium

                        }
                    }
                }

                ControlButton {
                    id: qualityButton
                    text: renderer.qualityLabel || "画质"
                    onClicked: qualityMenu.open()

                    Menu {
                        id: qualityMenu
                        y: -height - 12

                        MenuItem {
                            text: renderer.qualityLabel ? "当前: " + renderer.qualityLabel : "等待视频信息"
                            enabled: false
                        }
                    }
                }

                ControlButton {
                    id: subtitleButton
                    text: "字幕"
                    onClicked: subtitleMenu.open()

                    Menu {
                        id: subtitleMenu
                        y: -height - 12

                        Instantiator {
                            model: renderer.subtitleTracks
                            delegate: MenuItem {
                                required property var modelData
                                text: modelData.title
                                checkable: true
                                checked: renderer.subtitleId === modelData.id
                                onTriggered: renderer.setSubtitleId(modelData.id)
                            }

                            onObjectAdded: (index, object) => subtitleMenu.insertItem(index, object)
                            onObjectRemoved: (index, object) => subtitleMenu.removeItem(object)
                        }
                    }
                }

                ControlButton {
                    id: volumeButton
                    text: "音量 " + Math.round(renderer.volume)
                    onClicked: volumePopup.open()
                }

                Popup {
                    id: volumePopup
                    parent: volumeButton
                    x: (parent.width - width) / 2
                    y: -height - 12
                    width: 64
                    height: 176
                    padding: 12
                    modal: false
                    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
                    background: Rectangle {
                        radius: 14
                        color: "#F0121723"
                        border.width: 1
                        border.color: "#323A52"
                    }

                    Item {
                        id: volumeBar
                        anchors.horizontalCenter: parent.horizontalCenter
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        width: 24
                        property bool dragging: false
                        property real previewValue: renderer.volume
                        readonly property real shownValue: dragging ? previewValue : renderer.volume
                        readonly property real progress: Math.max(0, Math.min(1, shownValue / 100))

                        function updateFromPosition(positionY) {
                            const ratioFromTop = Math.max(0, Math.min(1, positionY / height))
                            previewValue = (1 - ratioFromTop) * 100
                        }

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 6
                            radius: 3
                            color: "#253047"

                            Rectangle {
                                anchors.bottom: parent.bottom
                                width: parent.width
                                height: parent.height * volumeBar.progress
                                radius: parent.radius
                                color: "#7EA8FF"
                            }
                        }

                        Rectangle {
                            anchors.horizontalCenter: parent.horizontalCenter
                            y: (1 - volumeBar.progress) * (volumeBar.height - height)
                            width: 14
                            height: 14
                            radius: 7
                            color: "#F5F8FF"
                            border.width: 2
                            border.color: "#6B96FF"
                        }

                        MouseArea {
                            anchors.fill: parent

                            onPressed: (mouse) => {
                                volumeBar.dragging = true
                                volumeBar.updateFromPosition(mouse.y)
                                renderer.setVolume(volumeBar.previewValue)
                            }

                            onPositionChanged: (mouse) => {
                                if (pressed) {
                                    volumeBar.updateFromPosition(mouse.y)
                                    renderer.setVolume(volumeBar.previewValue)
                                }
                            }

                            onReleased: {
                                volumeBar.dragging = false
                            }

                            onCanceled: {
                                volumeBar.dragging = false
                            }
                        }
                    }
                }

                ControlButton {
                    text: hostWindow && hostWindow.visibility === Window.FullScreen ? "退出全屏" : "全屏"
                    onClicked: {
                        if (!hostWindow) {
                            return
                        }

                        if (hostWindow.visibility === Window.FullScreen) {
                            hostWindow.showNormal()
                        } else {
                            hostWindow.showFullScreen()
                        }
                    }
                }
            }
        }
    }
}
