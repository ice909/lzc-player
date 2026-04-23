import QtQuick

Item {
    id: progressBar

    required property var player
    property bool dragging: false
    property bool awaitingSeek: false
    property real previewValue: 0
    readonly property real maxValue: Math.max(player.duration, 0.001)
    readonly property real shownValue: dragging || awaitingSeek ? previewValue : player.timePos
    readonly property real progress: Math.max(0, Math.min(1, shownValue / maxValue))
    readonly property real bufferProgress: Math.max(0, Math.min(1, player.bufferEnd / maxValue))
    readonly property bool hovered: progressHoverHandler.hovered || dragging
    readonly property real trackHeight: hovered ? 6 : 3
    readonly property real handleDiameter: 12
    readonly property real playedPixels: progressTrack.width * progress
    readonly property real playedPixelsSnapped: Math.round(playedPixels)
    readonly property real filledPixels: Math.max(
        0,
        Math.min(progressTrack.width, playedPixelsSnapped + (hovered && progress > 0 ? 1 : 0))
    )

    function updateFromPosition(positionX) {
        const ratio = Math.max(0, Math.min(1, positionX / width))
        previewValue = ratio * maxValue
    }

    Rectangle {
        id: progressTrack
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        height: progressBar.trackHeight
        radius: height / 2
        color: Qt.rgba(1, 1, 1, 0.15)
        clip: true

        Rectangle {
            width: parent.width * progressBar.bufferProgress
            height: parent.height
            radius: parent.radius
            color: Qt.rgba(1, 1, 1, 0.3)
        }

        Rectangle {
            width: progressBar.filledPixels
            height: parent.height
            radius: parent.radius
            color: "#3374DB"
        }

        Behavior on height {
            NumberAnimation {
                duration: 120
                easing.type: Easing.OutCubic
            }
        }
    }

    Rectangle {
        x: Math.round(
            Math.max(
                0,
                Math.min(
                    progressBar.width - width,
                    progressTrack.x + progressBar.playedPixelsSnapped - width / 2
                )
            )
        )
        anchors.verticalCenter: parent.verticalCenter
        visible: progressBar.hovered
        width: progressBar.handleDiameter
        height: progressBar.handleDiameter
        radius: 6
        color: "#FFFFFF"
    }

    HoverHandler {
        id: progressHoverHandler
        cursorShape: Qt.PointingHandCursor
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

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
            player.seekTo(progressBar.previewValue)
            progressBar.dragging = false
        }

        onCanceled: {
            progressBar.dragging = false
            progressBar.awaitingSeek = false
        }
    }

    Connections {
        target: player

        function onTimePosChanged() {
            if (
                progressBar.awaitingSeek
                && Math.abs(player.timePos - progressBar.previewValue) <= 0.5
            ) {
                progressBar.awaitingSeek = false
            }
        }
    }
}
