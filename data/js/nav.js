function switch_nav() {
  var x = document.getElementById("myTopnav");
  if (!x) return;
  var open = x.classList.toggle("responsive");
  var btn = x.querySelector(".nav-toggle");
  if (btn) {
    btn.setAttribute("aria-expanded", open ? "true" : "false");
    btn.setAttribute("aria-label", open ? "Close menu" : "Open menu");
  }
}

function close_nav() {
  var x = document.getElementById("myTopnav");
  if (!x || !x.classList.contains("responsive")) return;
  x.classList.remove("responsive");
  var btn = x.querySelector(".nav-toggle");
  if (btn) {
    btn.setAttribute("aria-expanded", "false");
    btn.setAttribute("aria-label", "Open menu");
  }
}

function toggle_password(input) {
  if (typeof input === "string") {
    input = document.getElementById(input);
  }
  if (!input) return;

  var el = document.getElementById(input.name + "_toggler");
  var img = el && el.tagName === "IMG" ? el : el && el.querySelector("img");

  if (input.type === "password") {
    input.type = "text";
    if (img) img.src = "/icons/eyeo.png";
    if (el && el.setAttribute) el.setAttribute("aria-label", "Hide password");
  } else {
    input.type = "password";
    if (img) img.src = "/icons/eyec.png";
    if (el && el.setAttribute) el.setAttribute("aria-label", "Show password");
  }
}

document.addEventListener("DOMContentLoaded", function () {
  var nav = document.getElementById("myTopnav");
  if (!nav) return;

  var btn = nav.querySelector(".nav-toggle");
  if (btn) {
    btn.setAttribute("aria-expanded", "false");
    btn.setAttribute("aria-controls", "myTopnav");
  }

  nav.addEventListener("click", function (e) {
    var t = e.target;
    if (t && t.closest && t.closest("a")) close_nav();
  });

  document.addEventListener("keydown", function (e) {
    if (e.key === "Escape") close_nav();
  });
});
