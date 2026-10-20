import QtQuick
import QtTest
import "../../qml/components/Args.js" as Args

TestCase {
    name: "Args"

    function test_split_data() {
        return [
            { tag: "empty", text: "", args: [] },
            { tag: "spaces only", text: "   ", args: [] },
            { tag: "words", text: "-f --fullscreen", args: ["-f", "--fullscreen"] },
            { tag: "runs of spaces", text: "  -a   -b ", args: ["-a", "-b"] },
            { tag: "double quotes", text: "-c \"My Config.ini\"", args: ["-c", "My Config.ini"] },
            { tag: "single quotes", text: "-c 'My Config.ini'", args: ["-c", "My Config.ini"] },
            { tag: "empty quotes", text: "-a \"\" -b", args: ["-a", "", "-b"] },
            { tag: "quote inside an argument", text: "--config=\"My Config.ini\" -f", args: ["--config=My Config.ini", "-f"] },
            { tag: "quoted parts together", text: "\"a b\"'c d'e", args: ["a bc de"] },
            { tag: "quotes inside other quotes", text: "\"it's\" 'say \"hi\"'", args: ["it's", "say \"hi\""] },
            { tag: "apostrophe", text: "--name=it's -f", args: ["--name=it's", "-f"] },
            { tag: "unclosed quote", text: "-a \"b c", args: ["-a", "\"b", "c"] },
        ]
    }

    function test_split(data) {
        compare(Args.split(data.text), data.args)
    }

    function test_join_data() {
        return [
            { tag: "empty", args: [], text: "" },
            { tag: "words", args: ["-f", "--fullscreen"], text: "-f --fullscreen" },
            { tag: "spaces", args: ["-c", "My Config.ini"], text: "-c \"My Config.ini\"" },
            { tag: "empty argument", args: ["-a", ""], text: "-a \"\"" },
            { tag: "apostrophe", args: ["it's"], text: "\"it's\"" },
            { tag: "double quotes", args: ["say \"hi\""], text: "'say \"hi\"'" },
        ]
    }

    function test_join(data) {
        compare(Args.join(data.args), data.text)
    }

    function test_roundTrip_data() {
        return [
            { tag: "words", args: ["-f", "--fullscreen"] },
            { tag: "spaces", args: ["-c", "My Config.ini", "-x"] },
            { tag: "empty argument", args: ["", "-a"] },
            { tag: "quotes", args: ["--config=My Config.ini", "it's", "say \"hi\"", "x\"y\"z"] },
        ]
    }

    function test_roundTrip(data) {
        compare(Args.split(Args.join(data.args)), data.args)
    }
}
