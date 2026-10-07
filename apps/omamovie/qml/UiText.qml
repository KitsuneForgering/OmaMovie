import QtQuick

// Text in the UI font. Plain Text items keep the font they were created with, so the Omarchy
// font (live, ui-design §10.1) reaches them through this binding; Controls inherit it from the
// window instead. The viewer never shows UI text, so nothing here touches picture colours.
Text {
    font.family: uiFont
}
