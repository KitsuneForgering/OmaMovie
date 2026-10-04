import QtQuick
import QtQuick.Shapes

// Our own monochrome set on a 24 px grid, 1.5 px stroke, tinted at runtime (ui-design §10.2).
// Drawn as SVG paths with Qt Quick Shapes until SVG files can ship through qt6-svg.
Item {
    id: icon
    property string name
    property color color: colors.foreground
    property real size: 18
    implicitWidth: size
    implicitHeight: size
    readonly property var paths: ({
        "back": "M15 6l-6 6 6 6",
        "undo": "M9 14L4 9l5-5M4 9h10a5.5 5.5 0 0 1 0 11h-3",
        "redo": "M15 14l5-5-5-5M20 9H10a5.5 5.5 0 0 0 0 11h3",
        "import": "M12 4v11M7.5 10.5L12 15l4.5-4.5M5 20h14",
        "export": "M12 15V4M7.5 8.5L12 4l4.5 4.5M5 20h14",
        "library": "M4 5h6v14H4zM14 5h6v6h-6zM14 15h6v4h-6z",
        "color": "M12 4a8 8 0 1 0 0 16a8 8 0 1 0 0-16zM12 4v16",
        "crop": "M7 3v14h14M3 7h14v14",
        "volume": "M4 9.5h3.5L12 6v12l-4.5-3.5H4zM15.5 9.5a3.5 3.5 0 0 1 0 5M18 7a7 7 0 0 1 0 10",
        "music": "M9 17.5V6l10-2v11.5M9 17.5a2.5 2.5 0 1 1-5 0a2.5 2.5 0 1 1 5 0zM19 15.5a2.5 2.5 0 1 1-5 0a2.5 2.5 0 1 1 5 0z",
        "speed": "M12 20a8 8 0 1 1 0-16a8 8 0 1 1 0 16zM12 12l3.5-3.5M12 4v2.5",
        "effects": "M12 4l1.8 6.2L20 12l-6.2 1.8L12 20l-1.8-6.2L4 12l6.2-1.8z",
        "overlay": "M4 4h11v11H4zM9 9h11v11H9z",
        "info": "M12 4a8 8 0 1 0 0 16a8 8 0 1 0 0-16zM12 11v5M12 8v.6",
        "play": "M8 5.5v13l10-6.5z",
        "pause": "M9 5.5v13M15 5.5v13",
        "stop": "M6 6h12v12H6z",
        "start": "M6 5v14M17 6l-7 6 7 6z",
        "end": "M18 5v14M7 6l7 6-7 6z",
        "backward": "M12 6l-7 6 7 6zM19 6l-7 6 7 6z",
        "forward": "M5 6l7 6-7 6zM12 6l7 6-7 6z",
        "previous": "M17 6l-7 6 7 6zM7 6v12",
        "next": "M7 6l7 6-7 6zM17 6v12",
        "append": "M4 7h9v10H4zM17 8v8M13 12h8",
        "insert": "M4 7h5v10H4zM15 7h5v10h-5zM12 5v14",
        "overwrite": "M4 7h16v10H4zM10 10h4v4h-4z",
        "split": "M12 4v16M4 8h5v8H4zM15 8h5v8h-5z",
        "rippleDelete": "M4 8h5v8H4zM15 8h5v8h-5zM10 10l4 4M14 10l-4 4",
        "detach": "M4 5h16v7H4zM5 15.5h2l1.5-2 2 5 2-6 2 4.5 1.5-1.5h3",
        "lift": "M4 8h16v9H4zM12 12V3M9 6l3-3 3 3",
        "fullscreen": "M4 9V4h5M15 4h5v5M20 15v5h-5M9 20H4v-5",
        "zoomIn": "M10.5 4a6.5 6.5 0 1 0 0 13a6.5 6.5 0 1 0 0-13zM15.5 15.5L20 20M7.5 10.5h6M10.5 7.5v6",
        "zoomOut": "M10.5 4a6.5 6.5 0 1 0 0 13a6.5 6.5 0 1 0 0-13zM15.5 15.5L20 20M7.5 10.5h6",
        "fit": "M4 12h16M7 9l-3 3 3 3M17 9l3 3-3 3"
    })
    Shape {
        width: 24
        height: 24
        scale: icon.size / 24
        transformOrigin: Item.TopLeft
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: icon.color
            strokeWidth: 1.5
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: icon.paths[icon.name] || "" }
        }
    }
}
