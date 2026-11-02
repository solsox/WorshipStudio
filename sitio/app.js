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
    if (cta) cta.textContent = "Descargar gratis para " + names[os];
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
