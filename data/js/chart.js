/* Live kiln chart — uPlot + /PIDKiln_chart.json (HTTP UI). CSV is durable archive only. */
(function () {
  var chartUrl = "/PIDKiln_chart.json";
  var POLL_MS = 10000;
  var host = document.getElementById("ProgramChart");
  if (!host || typeof uPlot === "undefined") return;

  var plot = null;
  var lastGen = null;
  var lastSince = 0;
  var pollTimer = null;
  var programPts = [];
  var livePts = [];
  var hasEnergy = false;

  function stroke(color, width) {
    return { stroke: color, width: width || 2 };
  }

  function makeOpts(wantEnergy) {
    var series = [
      {},
      Object.assign({ label: "Program", scale: "C" }, stroke("#6b8cae")),
      Object.assign({ label: "Kiln", scale: "C" }, stroke("#e87878")),
      Object.assign({ label: "Housing", scale: "C" }, stroke("#d4a05a"))
    ];
    var axes = [
      {
        stroke: "#9aa0a8",
        grid: { stroke: "rgba(61,68,80,0.9)" },
        ticks: { stroke: "#3d4450" }
      },
      {
        scale: "C",
        stroke: "#9aa0a8",
        grid: { stroke: "rgba(61,68,80,0.9)" },
        ticks: { stroke: "#3d4450" }
      }
    ];
    var scales = { x: { time: true }, C: { auto: true } };
    if (wantEnergy) {
      series.push(Object.assign({ label: "Energy", scale: "W" }, stroke("#7fa0c0", 1)));
      axes.push({
        scale: "W",
        side: 1,
        stroke: "#9aa0a8",
        grid: { show: false },
        ticks: { stroke: "#3d4450" }
      });
      scales.W = { auto: true };
    }
    return {
      width: Math.max(host.clientWidth || (host.parentElement && host.parentElement.clientWidth) || 600, 280),
      height: Math.max(Math.min(Math.round((host.clientWidth || 600) * 0.4), 320), 180),
      series: series,
      axes: axes,
      scales: scales,
      legend: { show: true }
    };
  }

  function buildData() {
    var map = {};
    function touch(t) {
      if (!map[t]) map[t] = { t: t, p: null, k: null, c: null, e: null };
      return map[t];
    }
    for (var i = 0; i < programPts.length; i++) {
      touch(programPts[i].t).p = programPts[i].y;
    }
    for (var j = 0; j < livePts.length; j++) {
      var row = touch(livePts[j].t);
      row.k = livePts[j].k;
      row.c = livePts[j].c;
      if (typeof livePts[j].e === "number") {
        row.e = livePts[j].e;
        hasEnergy = true;
      }
    }
    var times = Object.keys(map)
      .map(Number)
      .sort(function (a, b) {
        return a - b;
      });
    var xs = [];
    var yp = [];
    var yk = [];
    var yc = [];
    var ye = [];
    for (var n = 0; n < times.length; n++) {
      var r = map[times[n]];
      xs.push(r.t);
      yp.push(r.p);
      yk.push(r.k);
      yc.push(r.c);
      ye.push(r.e);
    }
    if (!xs.length) {
      xs = [Math.floor(Date.now() / 1000)];
      yp = [null];
      yk = [null];
      yc = [null];
      ye = [null];
    }
    var data = [xs, yp, yk, yc];
    if (hasEnergy) data.push(ye);
    return data;
  }

  function ensurePlot() {
    var want = hasEnergy;
    if (plot && plot.series.length === (want ? 5 : 4)) return;
    if (plot) {
      plot.destroy();
      plot = null;
      host.innerHTML = "";
    }
    plot = new uPlot(makeOpts(want), buildData(), host);
  }

  function applyPayload(payload, incremental) {
    if (!payload) return;
    if (lastGen !== null && payload.gen !== lastGen) {
      programPts = [];
      livePts = [];
      lastSince = 0;
      hasEnergy = false;
      incremental = false;
    }
    lastGen = payload.gen;

    if (!incremental && payload.program && payload.program.length) {
      programPts = payload.program.slice();
    }

    var pts = payload.points || [];
    for (var j = 0; j < pts.length; j++) {
      livePts.push(pts[j]);
      if (pts[j].t > lastSince) lastSince = pts[j].t;
    }

    ensurePlot();
    if (plot) plot.setData(buildData());
  }

  function fetchChart(full) {
    var url = chartUrl;
    if (!full && lastSince) url += "?since=" + lastSince;
    return fetch(url, { credentials: "same-origin", cache: "no-store" })
      .then(function (res) {
        if (!res.ok) throw new Error("chart " + res.status);
        return res.json();
      })
      .then(function (data) {
        applyPayload(data, !full && !!lastSince);
      })
      .catch(function (err) {
        if (typeof console !== "undefined" && console.warn) {
          console.warn("[PIDKiln chart]", err && err.message ? err.message : err);
        }
        if (host) host.setAttribute("data-chart-error", "1");
      });
  }

  function schedule() {
    pollTimer = setTimeout(function () {
      fetchChart(false).then(schedule);
    }, POLL_MS);
  }

  function onResize() {
    if (!plot) return;
    var w = Math.max(host.clientWidth || (host.parentElement && host.parentElement.clientWidth) || 600, 280);
    var h = Math.max(Math.min(Math.round(w * 0.4), 320), 180);
    plot.setSize({ width: w, height: h });
  }

  window.addEventListener("resize", onResize);
  window.pidkilnChartRefresh = function (full) {
    return fetchChart(!!full);
  };

  fetchChart(true).then(schedule);
})();
