(function () {
  "use strict";

  var SCHEMA = "pidkiln.program/v1";
  var MAX_TEMP = 1350;
  var MAX_STEPS = 40;
  var MAX_JSON = 10240;
  var MAX_FNAME = 20;
  var FULL_POWER = 9999;
  var VW = 360;
  var VH = 220;
  var PAD = { t: 18, r: 12, b: 28, l: 36 };
  var PLOT_W = VW - PAD.l - PAD.r;
  var PLOT_H = VH - PAD.t - PAD.b;

  var state = {
    program: {
      schema: SCHEMA,
      name: "new_program",
      description: "",
      assume_start_c: 25,
      steps: [],
    },
    filename: "new_program.json",
    selected: null,
    drag: null,
    message: "",
    messageBad: false,
  };

  function $(id) {
    return document.getElementById(id);
  }

  function assumeStart() {
    var a = state.program.assume_start_c;
    return a != null ? a : 25;
  }

  function clamp(v, lo, hi) {
    return Math.max(lo, Math.min(hi, v));
  }

  function stepRampMin(step, fromC) {
    if (step.rate_c_per_h != null) {
      if (!fromC && fromC !== 0) return 0;
      if (step.rate_c_per_h >= FULL_POWER) return 1;
      var delta = Math.abs(step.target_c - fromC);
      return delta
        ? clamp(Math.ceil((60 * delta) / step.rate_c_per_h), 1, 65535)
        : 1;
    }
    return step.ramp_min || 0;
  }

  function buildKeyframes(prog) {
    var pts = [{ t: 0, y: assumeStart(), i: -1, k: "s" }];
    var t = 0;
    var prev = assumeStart();
    for (var i = 0; i < prog.steps.length; i++) {
      var step = prog.steps[i];
      t += stepRampMin(step, prev);
      pts.push({ t: t, y: step.target_c, i: i, k: "r" });
      t += step.hold_min;
      pts.push({ t: t, y: step.target_c, i: i, k: "h" });
      prev = step.target_c;
    }
    return pts;
  }

  function profileStats(pts) {
    var dur = 0;
    var peak = 0;
    for (var i = 0; i < pts.length; i++) {
      if (pts[i].t > dur) dur = pts[i].t;
      if (pts[i].y > peak) peak = pts[i].y;
    }
    return { dur: dur, peak: peak };
  }

  function fmtDur(min) {
    min = Math.round(min);
    if (min < 60) return min + " min";
    var h = (min / 60) | 0;
    var m = min % 60;
    return m ? h + " h " + m + " min" : h + " h";
  }

  function rampFieldsFromDrag(step, fromC, rampMin) {
    rampMin = clamp(Math.round(rampMin), 0, 65535);
    var out = { target_c: step.target_c, hold_min: step.hold_min };
    if (step.rate_c_per_h != null) {
      out.rate_c_per_h =
        rampMin <= 0
          ? FULL_POWER
          : (function () {
              rampMin = clamp(Math.round(rampMin), 1, 65535);
              var delta = Math.abs(step.target_c - fromC);
              return delta
                ? clamp(Math.round((60 * delta) / rampMin), 1, 65535)
                : FULL_POWER;
            })();
    } else {
      out.ramp_min = rampMin;
    }
    return out;
  }

  function validateProgram(p) {
    var issues = [];
    if (!p || typeof p !== "object") return ["bad program"];
    if (p.schema !== SCHEMA) issues.push("bad schema");
    if (!Array.isArray(p.steps) || !p.steps.length) issues.push("need steps");
    if (p.steps && p.steps.length > MAX_STEPS) issues.push("too many steps");
    if (p.steps) {
      for (var i = 0; i < p.steps.length; i++) {
        var s = p.steps[i];
        var hasRate = s.rate_c_per_h != null;
        var hasRamp = s.ramp_min != null;
        if (hasRate === hasRamp) issues.push("step " + (i + 1) + " ramp");
        if (s.target_c < 0 || s.target_c > MAX_TEMP)
          issues.push("step " + (i + 1) + " temp");
      }
    }
    return issues;
  }

  function xTime(t, xMax) {
    return PAD.l + (t / xMax) * PLOT_W;
  }

  function yTemp(y, yMax) {
    return PAD.t + PLOT_H * (1 - y / yMax);
  }

  function prevTemps() {
    var t0 = assumeStart();
    var temps = [];
    for (var i = 0; i < state.program.steps.length; i++) {
      temps.push(t0);
      t0 = state.program.steps[i].target_c;
    }
    return temps;
  }

  function tailDurationAfter(stepIndex, temps) {
    var tail = 0;
    var prev = state.program.steps[stepIndex].target_c;
    for (var i = stepIndex + 1; i < state.program.steps.length; i++) {
      var step = state.program.steps[i];
      var from = temps[i] != null ? temps[i] : prev;
      tail += stepRampMin(step, from) + step.hold_min;
      prev = step.target_c;
    }
    return tail;
  }

  function patchStep(index, patch) {
    var cur = state.program.steps[index];
    var next = {
      target_c: patch.target_c != null ? patch.target_c : cur.target_c,
      hold_min: patch.hold_min != null ? patch.hold_min : cur.hold_min,
    };
    if (patch.rate_c_per_h != null) {
      next.rate_c_per_h = patch.rate_c_per_h;
    } else if (patch.ramp_min != null) {
      next.ramp_min = patch.ramp_min;
    } else if (cur.rate_c_per_h != null) {
      next.rate_c_per_h = cur.rate_c_per_h;
    } else {
      next.ramp_min = cur.ramp_min || 0;
    }
    state.program.steps[index] = next;
  }

  function numInput(field, opts) {
    return (
      '<input type="number" data-f="' +
      field +
      '" data-i="' +
      opts.i +
      '" min="' +
      opts.min +
      '" max="' +
      opts.max +
      '" value="' +
      opts.val +
      '">'
    );
  }

  function chartScale() {
    var pts = buildKeyframes(state.program);
    var st = profileStats(pts);
    var d = state.drag;
    return {
      pts: pts,
      stats: st,
      xMax: d ? d.xMax : Math.max(st.dur, 1),
      yMax: d ? d.yMax : Math.max(st.peak * 1.08, 50),
    };
  }

  function renderChart() {
    var svg = $("pe-chart");
    if (!svg) return;

    var sc = chartScale();
    var pts = sc.pts;
    var xMax = sc.xMax;
    var yMax = sc.yMax;
    var st = sc.stats;
    var poly = "";

    for (var i = 0; i < pts.length; i++) {
      if (i) poly += " ";
      poly +=
        xTime(pts[i].t, xMax).toFixed(1) +
        "," +
        yTemp(pts[i].y, yMax).toFixed(1);
    }

    var html =
      '<defs><linearGradient id="peFill" x1="0" y1="0" x2="0" y2="1">' +
      '<stop offset="0%" stop-color="#d4a05a" stop-opacity=".4"/>' +
      '<stop offset="100%" stop-color="#d4a05a" stop-opacity="0"/></linearGradient></defs>';

    for (var g = 0; g < 5; g++) {
      var gy = yTemp((g / 4) * yMax, yMax);
      html +=
        '<line class="pe-grid" x1="' +
        PAD.l +
        '" x2="' +
        (VW - PAD.r) +
        '" y1="' +
        gy +
        '" y2="' +
        gy +
        '"/>';
    }

    var x0 = xTime(0, xMax);
    var yBase = yTemp(0, yMax);
    var xEnd = xTime(pts[pts.length - 1].t, xMax);

    html +=
      '<polygon class="pe-area" points="' +
      x0 +
      "," +
      yBase +
      " " +
      poly +
      " " +
      xEnd +
      "," +
      yBase +
      '"/>';
    html += '<polyline class="pe-line" points="' + poly + '"/>';

    for (var p = 0; p < pts.length; p++) {
      var kp = pts[p];
      var cx = xTime(kp.t, xMax);
      var cy = yTemp(kp.y, yMax);
      var sel = state.selected === kp.i;

      if (kp.k === "r") {
        if (sel) {
          html +=
            '<circle class="pe-ring" cx="' + cx + '" cy="' + cy + '" r="14"/>';
        }
        html +=
          '<circle class="pe-ramp' +
          (sel ? " selected" : "") +
          '" data-kind="r" data-i="' +
          kp.i +
          '" cx="' +
          cx +
          '" cy="' +
          cy +
          '" r="10"/>';
      } else if (kp.k === "h") {
        if (sel) {
          html +=
            '<circle class="pe-ring" cx="' + cx + '" cy="' + cy + '" r="14"/>';
        }
        html +=
          '<rect class="pe-hold' +
          (sel ? " selected" : "") +
          '" data-kind="h" data-i="' +
          kp.i +
          '" x="' +
          (cx - 7) +
          '" y="' +
          (cy - 7) +
          '" width="14" height="14" transform="rotate(45 ' +
          cx +
          " " +
          cy +
          ')"/>';
      }
    }

    if (state.drag) {
      var dragPt;
      for (var d = 0; d < pts.length; d++) {
        if (pts[d].i === state.drag.i && pts[d].k === state.drag.k) {
          dragPt = pts[d];
          break;
        }
      }
      var step = state.program.steps[state.drag.i];
      if (step && dragPt) {
        var tip =
          state.drag.k === "h" ? fmtDur(step.hold_min) : step.target_c + " °C";
        var tw = Math.max(56, 7 * tip.length + 16);
        var tx = clamp(
          xTime(dragPt.t, xMax),
          PAD.l + tw / 2,
          VW - PAD.r - tw / 2
        );
        var ty = Math.max(PAD.t + 14, yTemp(dragPt.y, yMax) - 16);
        html +=
          '<rect class="pe-tip-bg" x="' +
          (tx - tw / 2) +
          '" y="' +
          (ty - 14) +
          '" width="' +
          tw +
          '" height="18" rx="3"/>';
        html +=
          '<text class="pe-tip" x="' +
          tx +
          '" y="' +
          ty +
          '" text-anchor="middle">' +
          tip +
          "</text>";
      }
    }

    html += '<text class="pe-axis" x="' + PAD.l + '" y="' + (VH - 8) + '">0</text>';
    html +=
      '<text class="pe-axis" x="' +
      (VW - PAD.r) +
      '" y="' +
      (VH - 8) +
      '" text-anchor="end">' +
      fmtDur(st.dur) +
      "</text>";
    html +=
      '<text class="pe-axis" x="4" y="' +
      (yTemp(yMax, yMax) + 4) +
      '">' +
      Math.round(yMax) +
      "°</text>";

    svg.innerHTML = html;

    $("pe-meta-dur").textContent = fmtDur(st.dur);
    $("pe-meta-peak").textContent = st.peak + " °C";
    $("pe-meta-steps").textContent = String(state.program.steps.length);
    $("pe-sel-label").textContent =
      state.selected != null ? "Step " + (state.selected + 1) : "None";

    var full = state.program.steps.length >= MAX_STEPS;
    var empty = !state.program.steps.length;
    $("pe-add").disabled = full;
    $("pe-rm").disabled = state.selected == null || empty;
    if (empty) $("pe-empty-add").removeAttribute("hidden");
    else $("pe-empty-add").setAttribute("hidden", "");
  }

  function renderStepsForm() {
    var prog = state.program;
    $("pe-fname").value = state.filename;
    $("pe-name").value = prog.name || "";
    $("pe-desc").value = prog.description || "";
    $("pe-assume").value =
      prog.assume_start_c != null ? prog.assume_start_c : "";

    var box = $("pe-steps");
    var html = "";
    for (var i = 0; i < prog.steps.length; i++) {
      var step = prog.steps[i];
      var byRate = step.rate_c_per_h != null;
      html +=
        '<details class="pe-step"' +
        (state.selected === i ? " open" : "") +
        ' data-i="' +
        i +
        '"><summary>Step ' +
        (i + 1) +
        " — " +
        step.target_c +
        ' °C</summary><div class="pe-step-grid">';
      html +=
        "<label>Target °C" +
        numInput("target_c", { i: i, min: 0, max: MAX_TEMP, val: step.target_c }) +
        "</label>";
      html +=
        '<label>Ramp by<select data-f="mode" data-i="' +
        i +
        '"><option value="ramp_min"' +
        (byRate ? "" : " selected") +
        '>Minutes</option><option value="rate_c_per_h"' +
        (byRate ? " selected" : "") +
        ">°C/h</option></select></label>";
      html += byRate
        ? "<label>Rate °C/h" +
          numInput("rate_c_per_h", {
            i: i,
            min: 1,
            max: 65535,
            val: step.rate_c_per_h,
          }) +
          "</label>"
        : "<label>Ramp min" +
          numInput("ramp_min", {
            i: i,
            min: 0,
            max: 65535,
            val: step.ramp_min || 0,
          }) +
          "</label>";
      html +=
        "<label>Hold min" +
        numInput("hold_min", { i: i, min: 0, max: 65535, val: step.hold_min }) +
        "</label></div></details>";
    }
    box.innerHTML = html;

    var issues = validateProgram(prog);
    var elIssues = $("pe-issues");
    elIssues.hidden = !issues.length;
    elIssues.textContent = issues.join("; ");

    var elMsg = $("pe-msg");
    if (state.message) {
      elMsg.hidden = false;
      elMsg.textContent = state.message;
      elMsg.className = "status-line" + (state.messageBad ? " bad" : " ok");
    } else {
      elMsg.hidden = true;
    }
  }

  function refresh() {
    renderChart();
    renderStepsForm();
  }

  function svgPoint(evt, svg) {
    var r = svg.getBoundingClientRect();
    return {
      x: ((evt.clientX - r.left) * VW) / r.width,
      y: ((evt.clientY - r.top) * VH) / r.height,
    };
  }

  function onPointerMove(evt, svg) {
    var drag = state.drag;
    if (!drag) return;

    var pt = svgPoint(evt, svg);
    if (pt.x > VW - PAD.r && drag.xMax) {
      drag.xMax = Math.max(drag.xMax, drag.xMax * ((pt.x - PAD.l) / PLOT_W));
    }
    if (pt.y < PAD.t && drag.yMax) {
      var frac = 1 - (pt.y - PAD.t) / PLOT_H;
      if (frac > 1) drag.yMax = Math.max(drag.yMax, drag.yMax * frac);
    }

    var tMin = Math.max(0, ((pt.x - PAD.l) / PLOT_W) * drag.xMax);
    var step = state.program.steps[drag.i];
    var temps = prevTemps();

    if (drag.k === "r") {
      var ramp = Math.max(0, tMin - drag.segStart);
      var temp = Math.round(
        clamp((1 - (pt.y - PAD.t) / PLOT_H) * drag.yMax, 0, MAX_TEMP)
      );
      var fields = rampFieldsFromDrag(
        { target_c: temp, hold_min: step.hold_min, ramp_min: step.ramp_min, rate_c_per_h: step.rate_c_per_h },
        drag.stepFromC,
        ramp
      );
      var endT = drag.segStart + ramp + step.hold_min + tailDurationAfter(drag.i, temps);
      if (endT > drag.xMax) drag.xMax = endT;
      if (temp * 1.08 > drag.yMax) drag.yMax = Math.max(drag.yMax, temp * 1.08);
      patchStep(drag.i, fields);
    } else {
      var hold = clamp(Math.round(tMin - drag.rampEnd), 0, 65535);
      var endH = drag.rampEnd + hold + tailDurationAfter(drag.i, temps);
      if (endH > drag.xMax) drag.xMax = endH;
      patchStep(drag.i, { hold_min: hold });
    }
    refresh();
  }

  function endDrag() {
    state.drag = null;
    refresh();
  }

  function addStep() {
    if (state.program.steps.length >= MAX_STEPS) return;

    if (!state.program.steps.length) {
      var start = assumeStart();
      state.program.steps.push({
        target_c: clamp(Math.max(start + 50, 100), 0, MAX_TEMP),
        ramp_min: 30,
        hold_min: 10,
      });
      state.selected = 0;
      refresh();
      return;
    }

    var at = state.selected != null ? state.selected : state.program.steps.length - 1;
    var cur = state.program.steps[at];
    var nextStep = state.program.steps[at + 1];
    var target = nextStep
      ? Math.round((cur.target_c + nextStep.target_c) / 2)
      : clamp(cur.target_c + 50, 0, MAX_TEMP);
    state.program.steps.splice(at + 1, 0, {
      target_c: target,
      ramp_min: 20,
      hold_min: cur.hold_min,
    });
    state.selected = at + 1;
    refresh();
  }

  function showError(msg) {
    state.message = msg;
    state.messageBad = true;
    refresh();
  }

  function serializeProgram(prog) {
    var out = { schema: SCHEMA };
    if (prog.name) out.name = String(prog.name).slice(0, 31);
    if (prog.description) out.description = String(prog.description).slice(0, 159);
    if (prog.assume_start_c != null && prog.assume_start_c !== "")
      out.assume_start_c = +prog.assume_start_c;
    out.steps = [];
    for (var i = 0; i < prog.steps.length; i++) {
      var s = prog.steps[i];
      var row = { target_c: +s.target_c, hold_min: +s.hold_min };
      if (s.rate_c_per_h != null) row.rate_c_per_h = +s.rate_c_per_h;
      else row.ramp_min = +(s.ramp_min || 0);
      out.steps.push(row);
    }
    return JSON.stringify(out) + "\n";
  }

  function validFilename(name) {
    return !!(name && name.length <= MAX_FNAME && /^[A-Za-z0-9._]+$/.test(name));
  }

  function saveProgram() {
    var issues = validateProgram(state.program);
    state.message = "";
    if (issues.length) {
      showError(issues.join("; "));
      return;
    }

    var fname =
      state.filename ||
      (function () {
        var base = (state.program.name || "program")
          .replace(/[^a-zA-Z0-9._]/g, "_")
          .slice(0, 15);
        if (!/\.json$/i.test(base)) base += ".json";
        return base.slice(0, MAX_FNAME);
      })();

    if (!validFilename(fname)) {
      showError("bad filename");
      return;
    }

    var json = serializeProgram(state.program);
    if (json.length > MAX_JSON) {
      showError("over 10KiB");
      return;
    }

    var body = new FormData();
    body.append("upload", new Blob([json], { type: "application/json" }), fname);
    fetch("/upload", { method: "POST", body: body, credentials: "same-origin" })
      .then(function (res) {
        if (!res.ok) throw new Error("upload " + res.status);
        location.href = "/programs/";
      })
      .catch(function (err) {
        showError(err.message || "upload failed");
      });
  }

  function beginDrag(kind, index, evt) {
    state.selected = index;
    var temps = prevTemps();
    var t = 0;
    var segStart = 0;
    var rampEnd = 0;
    for (var a = 0; a <= index; a++) {
      var step = state.program.steps[a];
      var ramp = stepRampMin(step, temps[a]);
      if (a === index) {
        segStart = t;
        rampEnd = t + ramp;
        break;
      }
      t += ramp + step.hold_min;
    }
    var stats = profileStats(buildKeyframes(state.program));
    state.drag = {
      k: kind,
      i: index,
      segStart: segStart,
      rampEnd: rampEnd,
      stepFromC: temps[index],
      xMax: Math.max(stats.dur, 1),
      yMax: Math.max(stats.peak * 1.08, 50),
    };
    try {
      evt.target.setPointerCapture(evt.pointerId);
    } catch (e) {
      /* Older browsers may lack setPointerCapture; drag still works via move/up. */
    }
    evt.preventDefault();
    refresh();
  }

  function on(el, type, fn) {
    if (el) el.addEventListener(type, fn);
  }

  function init() {
    var chart = $("pe-chart");
    chart.setAttribute("viewBox", "0 0 " + VW + " " + VH);

    on(chart, "pointerdown", function (evt) {
      var kind = evt.target.getAttribute("data-kind");
      var idx = evt.target.getAttribute("data-i");
      if (kind != null && idx != null) beginDrag(kind, +idx, evt);
    });
    on(chart, "pointermove", function (evt) {
      onPointerMove(evt, chart);
    });
    on(chart, "pointerup", endDrag);
    on(chart, "pointercancel", endDrag);

    on($("pe-add"), "click", function (e) {
      e.preventDefault();
      addStep();
    });
    on($("pe-empty-add"), "click", function (e) {
      e.preventDefault();
      addStep();
    });
    on($("pe-rm"), "click", function (e) {
      e.preventDefault();
      if (state.selected == null || !state.program.steps.length) return;
      var at = state.selected;
      state.program.steps.splice(at, 1);
      state.selected = state.program.steps.length
        ? Math.min(at, state.program.steps.length - 1)
        : null;
      refresh();
    });
    on($("pe-save"), "click", function (e) {
      e.preventDefault();
      saveProgram();
    });

    on($("pe-fname"), "change", function (e) {
      state.filename = e.target.value;
    });
    on($("pe-name"), "input", function (e) {
      state.program.name = e.target.value;
    });
    on($("pe-desc"), "input", function (e) {
      state.program.description = e.target.value;
    });
    on($("pe-assume"), "input", function (e) {
      var v = e.target.value;
      state.program.assume_start_c = v === "" ? undefined : parseInt(v, 10) || 0;
      renderChart();
    });

    on($("pe-steps"), "input", function (evt) {
      var el = evt.target;
      var field = el.getAttribute("data-f");
      var idx = +el.getAttribute("data-i");
      if (field == null || isNaN(idx)) return;

      if (field === "mode") {
        var cur = state.program.steps[idx];
        state.program.steps[idx] =
          el.value === "rate_c_per_h"
            ? {
                target_c: cur.target_c,
                hold_min: cur.hold_min,
                rate_c_per_h: cur.rate_c_per_h || 120,
              }
            : {
                target_c: cur.target_c,
                hold_min: cur.hold_min,
                ramp_min: cur.ramp_min != null ? cur.ramp_min : 30,
              };
      } else {
        var patch = {};
        patch[field] = parseInt(el.value, 10) || 0;
        patchStep(idx, patch);
      }
      state.selected = idx;
      refresh();
    });

    on($("pe-steps"), "toggle", function (evt) {
      var el = evt.target;
      if (el.tagName === "DETAILS" && el.open) {
        state.selected = +el.getAttribute("data-i");
        renderChart();
      }
    });

    var progName = new URLSearchParams(location.search).get("prog");
    if (progName) {
      fetch("/programs/" + encodeURIComponent(progName), {
        credentials: "same-origin",
      })
        .then(function (res) {
          if (!res.ok) throw new Error("load " + res.status);
          return res.text();
        })
        .then(function (text) {
          var data = JSON.parse(text);
          var issues = validateProgram(data);
          if (issues.length) throw new Error(issues.join("; "));
          state.program = data;
          state.filename = progName.slice(0, MAX_FNAME);
          state.selected = 0;
          state.message = "";
          refresh();
        })
        .catch(function (err) {
          showError(err.message || "load failed");
        });
    } else {
      refresh();
    }
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", init);
  } else {
    init();
  }
})();