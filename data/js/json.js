const valuesUrl = "/PIDKiln_vars.json";
const POLL_MS = 5000;

var program_status = 0;
var pollTimer = null;

function setDisabled(id, disabled) {
  var el = document.getElementById(id);
  if (el) el.disabled = disabled;
}

function setButtonValue(id, value) {
  var el = document.getElementById(id);
  if (el) el.value = value;
}

function dis_all_bttn() {
  setDisabled("start_bttn", true);
  setDisabled("end_bttn", true);
  setDisabled("pause_bttn", true);
  setDisabled("abort_bttn", true);
}

function change_program_status(ns) {
  if (ns == 0) {
    dis_all_bttn();
  } else if (ns == 2 || ns == 6) {
    dis_all_bttn();
    setDisabled("pause_bttn", false);
    setDisabled("end_bttn", false);
    setDisabled("abort_bttn", false);
    if (typeof chart_update_id !== "undefined" && !chart_update_id) {
      chart_update_id = setTimeout(chart_update, 30000);
    }
  } else if (ns == 3) {
    dis_all_bttn();
    setDisabled("start_bttn", false);
    setButtonValue("start_bttn", "Resume program");
    setDisabled("end_bttn", false);
    setDisabled("abort_bttn", false);
    if (typeof chart_update_id !== "undefined" && !chart_update_id) {
      chart_update_id = setTimeout(chart_update, 30000);
    }
  } else {
    dis_all_bttn();
    setButtonValue("start_bttn", "Start program");
    setDisabled("start_bttn", false);
    if (typeof chart_update_id !== "undefined") {
      clearTimeout(chart_update_id);
      chart_update_id = null;
    }
  }
  program_status = ns;
}

function setFieldValue(selector, value) {
  var el = document.querySelector(selector);
  if (!el) return;
  if ("value" in el && el.tagName !== "OUTPUT") {
    el.value = value;
  } else {
    el.textContent = value;
  }
}

function executeQuery() {
  fetch(valuesUrl, { credentials: "same-origin", cache: "no-store" })
    .then(function (res) {
      if (!res.ok) throw new Error("status " + res.status);
      return res.json();
    })
    .then(function (data) {
      if (data.program_status != program_status) {
        change_program_status(data.program_status);
      }
      if (data.pidkiln && data.pidkiln.forEach) {
        data.pidkiln.forEach(function (el) {
          setFieldValue(el.html_id, el.value);
        });
      }
    })
    .catch(function () {
      /* keep last values; retry on next poll */
    })
    .then(function () {
      pollTimer = setTimeout(executeQuery, POLL_MS);
    });
}

change_program_status(1);

if (document.readyState === "loading") {
  document.addEventListener("DOMContentLoaded", executeQuery);
} else {
  executeQuery();
}
