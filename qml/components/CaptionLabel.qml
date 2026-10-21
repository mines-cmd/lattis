import QtQuick
import QtQuick.Controls.Basic

Label {
    font.weight: Theme.font.labelWeight
    font.capitalization: Theme.font.labelUppercase ? Font.AllUppercase : Font.MixedCase
    font.letterSpacing: Theme.font.labelLetterSpacing
}
