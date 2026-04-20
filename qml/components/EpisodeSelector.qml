import QtQuick
import QtQuick.Controls

Item {
    id: episodeSelector

    required property var player
    readonly property bool panelVisible: episodePopup.visible
    readonly property real popupEdgePadding: 12

    visible: player.hasPlaylist
    implicitWidth: visible ? 40 : 0
    implicitHeight: Math.max(24, episodeButtonLabel.implicitHeight)

    Timer {
        id: closeTimer
        interval: 150
        onTriggered: episodePopup.close()
    }

    Popup {
        id: episodePopup
        parent: Overlay.overlay
        x: {
            if (!parent) {
                return 0
            }

            const preferredX = episodeTrigger.mapToItem(
                parent,
                (episodeTrigger.width - width) / 2,
                0
            ).x
            const maxX = Math.max(
                episodeSelector.popupEdgePadding,
                parent.width - width - episodeSelector.popupEdgePadding
            )

            return Math.max(
                episodeSelector.popupEdgePadding,
                Math.min(preferredX, maxX)
            )
        }
        y: parent
            ? episodeTrigger.mapToItem(parent, 0, -height - 8).y
            : 0
        width: 280
        height: episodeContentColumn.implicitHeight + 16
        padding: 0
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent

        HoverHandler {
            onHoveredChanged: hovered ? closeTimer.stop() : closeTimer.start()
        }

        background: Rectangle {
            radius: 10
            color: "#141924"
            border.width: 1
            border.color: Qt.rgba(1, 1, 1, 0.15)
        }

        contentItem: Column {
            id: episodeContentColumn
            anchors.fill: parent
            anchors.margins: 8
            spacing: 8

            Item {
                width: parent.width
                height: episodePanelTitle.implicitHeight

                Text {
                    id: episodePanelTitle
                    anchors.left: parent.left
                    text: "选集"
                    color: "#FFFFFF"
                    font.pixelSize: 14
                    font.weight: Font.Medium
                }

                Text {
                    anchors.right: parent.right
                    text: "共 " + episodeSelector.player.playlistCount + " 集"
                    color: Qt.rgba(203 / 255, 213 / 255, 224 / 255, 0.75)
                    font.pixelSize: 12
                    font.weight: Font.Medium
                }
            }

            ScrollView {
                id: episodeScrollView
                width: parent.width
                height: Math.min(
                    episodeOptionsColumn.implicitHeight,
                    360 - 16 - episodePanelTitle.implicitHeight - episodeContentColumn.spacing
                )
                clip: true
                ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                contentWidth: availableWidth

                Column {
                    id: episodeOptionsColumn
                    width: episodeScrollView.availableWidth
                    spacing: 4

                    Repeater {
                        model: episodeSelector.player.playlistItems

                        delegate: Rectangle {
                            id: episodeOption
                            required property var modelData
                            required property int index
                            readonly property bool selected: episodeSelector.player.playlistIndex === index
                            readonly property string episodeName: modelData.name || ("第 " + String(index + 1) + " 集")
                            readonly property string durationText:
                                modelData.duration > 0
                                ? Math.round(Number(modelData.duration) / 60) + " 分钟"
                                : ""

                            width: parent ? parent.width : 248
                            height: durationText.length > 0 ? 56 : 40
                            radius: 6
                            color: episodeOptionMouseArea.containsMouse ? Qt.rgba(1, 1, 1, 0.04) : "transparent"

                            Column {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.leftMargin: 10
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                spacing: durationText.length > 0 ? 3 : 0

                                Text {
                                    width: parent.width
                                    color: episodeOption.selected ? Qt.rgba(33 / 255, 115 / 255, 223 / 255, 1) : "#CBD5E0"
                                    font.pixelSize: 14
                                    font.weight: Font.Medium
                                    text: episodeOption.episodeName
                                    elide: Text.ElideRight
                                }

                                Text {
                                    visible: episodeOption.durationText.length > 0
                                    width: parent.width
                                    color: Qt.rgba(203 / 255, 213 / 255, 224 / 255, 0.8)
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    text: episodeOption.durationText
                                    elide: Text.ElideRight
                                }
                            }

                            MouseArea {
                                id: episodeOptionMouseArea
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: {
                                    episodeSelector.player.playEpisode(episodeOption.index)
                                    episodePopup.close()
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Item {
        id: episodeTrigger
        anchors.centerIn: parent
        width: parent.width
        height: parent.height

        HoverHandler {
            id: episodeButtonHoverHandler
            cursorShape: Qt.PointingHandCursor
            onHoveredChanged: {
                if (hovered) {
                    closeTimer.stop()
                    episodePopup.open()
                } else {
                    closeTimer.start()
                }
            }
        }

        Text {
            id: episodeButtonLabel
            anchors.centerIn: parent
            text: "选集"
            color: episodeButtonHoverHandler.hovered || episodeSelector.panelVisible
                ? "#FFFFFF"
                : "#CBD5E0"
            font.pixelSize: 14
            font.weight: Font.Medium
        }
    }
}
