/* Tabs, download for the visitor's system and small touches. No tracking. */
(function () {
  var ua = navigator.userAgent || "";
  var platform = (navigator.userAgentData && navigator.userAgentData.platform) || navigator.platform || "";
  var os = /iPhone|iPad|Android/i.test(ua) ? "" : /Mac/i.test(platform + ua) ? "mac" : /Win/i.test(platform + ua) ? "win" : /Linux|X11/i.test(platform + ua) ? "linux" : "";
  var names = { mac: "Mac", win: "Windows", linux: "Linux" };
  if (os) {
    var card = document.getElementById("dl-" + os);
    if (card) card.classList.add("is-detected");
    var cta = document.getElementById("cta-principal");
    if (cta) {
      /* The system's own logo and name on the button, Raycast style. */
      var icons = {
        mac: '<path fill="currentColor" d="M16.4 12.6c0-2.4 2-3.5 2-3.6-1.1-1.6-2.8-1.8-3.4-1.8-1.4-.1-2.8.9-3.5.9s-1.8-.8-3-.8C6.9 7.3 5 8.6 5 11.4c0 1.7.6 3.5 1.4 4.7.7 1 1.5 2.1 2.5 2.1s1.4-.7 2.6-.7 1.6.7 2.6.7 1.8-1.1 2.4-2c.8-1.1 1.1-2.2 1.1-2.2s-2.2-.9-2.2-3.4zM14.5 5.6c.6-.7.9-1.6.8-2.6-.8 0-1.8.5-2.4 1.2-.5.6-1 1.6-.9 2.5.9.1 1.8-.4 2.5-1.1z"/>',
        win: '<path fill="currentColor" d="M3 5.5 10 4.5v7H3zM11 4.4 21 3v8.5H11zM3 12.5h7v7l-7-1zM11 12.5h10V21l-10-1.4z"/>',
        linux: '<path fill="currentColor" d="M12 2c-2 0-3.3 1.6-3.3 3.8 0 1.1.2 1.9-.4 3-.9 1.4-2.6 3.1-2.6 5.6 0 1.3.5 2.4 1.3 3.3h10c.8-.9 1.3-2 1.3-3.3 0-2.5-1.7-4.2-2.6-5.6-.6-1.1-.4-1.9-.4-3C15.3 3.6 14 2 12 2zm-1.4 3.2a.7.7 0 1 1 0 1.4.7.7 0 0 1 0-1.4zm2.8 0a.7.7 0 1 1 0 1.4.7.7 0 0 1 0-1.4zM12 7.6c.8 0 1.6.4 1.6.8s-.8.9-1.6.9-1.6-.5-1.6-.9.8-.8 1.6-.8zM7 19c-1.2.2-2 .7-2 1.3C5 21.2 6.3 22 8 22c1.2 0 2-.5 2.3-1.2H7.2zm10 0h-.2-3.1c.3.7 1.1 1.2 2.3 1.2 1.7 0 3-.8 3-1.7 0-.6-.8-1.1-2-1.3z"/>'
      };
      var notes = { mac: "Gratis · macOS 12 o posterior · Apple Silicon", win: "Gratis · Windows 10 y 11 · 64 bits", linux: "Gratis · Ubuntu 24.04 y similares" };
      var icon = document.getElementById("cta-icon");
      if (icon) icon.innerHTML = icons[os];
      var text = document.getElementById("cta-text");
      if (text) text.textContent = "Descargar para " + names[os];
      var note = document.getElementById("cta-note");
      if (note) note.textContent = notes[os];
      /* One click downloads; the install steps stay below. */
      if (card) cta.href = card.href;
      cta.addEventListener("click", function () {
        setTimeout(function () { location.hash = "descargar"; }, 400);
      });
    }
  }

  /* Accessible tabs (arrow keys move between them). */
  var tabs = Array.prototype.slice.call(document.querySelectorAll('[role="tab"]'));
  function select(tab) {
    tabs.forEach(function (t) {
      var on = t === tab;
      t.setAttribute("aria-selected", on ? "true" : "false");
      t.tabIndex = on ? 0 : -1;
      var panel = document.getElementById(t.getAttribute("aria-controls"));
      if (panel) panel.hidden = !on;
    });
  }
  tabs.forEach(function (tab, i) {
    tab.addEventListener("click", function () { select(tab); });
    tab.addEventListener("keydown", function (e) {
      var next = e.key === "ArrowRight" ? i + 1 : e.key === "ArrowLeft" ? i - 1 : null;
      if (next === null) return;
      e.preventDefault();
      var t = tabs[(next + tabs.length) % tabs.length];
      select(t);
      t.focus();
    });
  });

  var nav = document.querySelector(".nav");
  var onScroll = function () { if (nav) nav.classList.toggle("is-scrolled", window.scrollY > 8); };
  window.addEventListener("scroll", onScroll, { passive: true });
  onScroll();

  if ("IntersectionObserver" in window) {
    var items = document.querySelectorAll(".feat, .tile, .steps li, .dl, .card, .panel, .vs");
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (entry) {
        if (entry.isIntersecting) { entry.target.classList.add("is-visible"); io.unobserve(entry.target); }
      });
    }, { rootMargin: "0px 0px -6% 0px" });
    items.forEach(function (el) { el.classList.add("reveal"); io.observe(el); });
  }
})();

/* Phone / tablet switch */
(function () {
  var buttons = document.querySelectorAll(".switch [data-device]");
  [].forEach.call(buttons, function (button) {
    button.addEventListener("click", function () {
      var device = button.dataset.device;
      [].forEach.call(buttons, function (b) { b.setAttribute("aria-selected", b === button ? "true" : "false"); });
      [].forEach.call(document.querySelectorAll(".devices .device"), function (d) { d.classList.toggle("is-on", d.dataset.device === device); });
    });
  });
})();
