import QtQuick

import "ColorHarmony.js" as Harmony

// The functional color groups of one settings page (or one widget): give every ColorSpecButton of the page
// `groups: theGroups` and each registers its spec and source color here under its `groupKey`, so colors with the
// same group name are computed from that group's primary and secondary (ColorHarmony.js) and every button,
// wheel and preview follows when one of them changes. `label` names the color in the group's wheel.
QtObject {
    id: root

    // key → {key, label, spec, base}, in registration order (the first primary of a group wins)
    property var entries: ({})
    property int revision: 0
    property int counter: 0

    readonly property var list: {
        revision; // re-evaluate when an entry changes
        return Object.keys(entries).map(k => entries[k]);
    }
    readonly property var resolved: Harmony.resolveEntries(list)
    readonly property var names: Harmony.groupNames(list)

    function newKey() {
        counter += 1;
        return "color" + counter;
    }
    function register(key, label, spec, base) {
        const e = entries;
        e[key] = { key: key, label: label, spec: spec, base: base };
        entries = e;
        revision += 1;
    }
    function unregister(key) {
        if (!(key in entries))
            return;
        const e = entries;
        delete e[key];
        entries = e;
        revision += 1;
    }
    // The page as it would be with `key` set to `spec` (a dialog's preview): {list, resolved}
    function resolveWith(key, label, spec, base) {
        let found = false;
        const l = list.map(e => {
            if (e.key !== key)
                return e;
            found = true;
            return { key: key, label: label, spec: spec, base: base };
        });
        if (!found)
            l.push({ key: key, label: label, spec: spec, base: base });
        return { list: l, resolved: Harmony.resolveEntries(l) };
    }
}
