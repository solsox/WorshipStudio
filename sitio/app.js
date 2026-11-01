/* Highlights the download for the visitor's system and adds small touches.
 * No tracking, no cookies. */
(function () {
  var ua = navigator.userAgent || "";
  var platform = (navigator.userAgentData && navigator.userAgentData.platform) || navigator.platform || "";
  var os = /Mac|iPhone|iPad/i.test(platform + ua) ? "mac" : /Win/i.test(platform + ua) ? "win" : /Linux|X11/i.test(platform + ua) ? "linux" : "";

  var detected = document.getElementById("dl-" + os);
  if (detected) detected.classList.add("is-detected");

  var cta = document.getElementById("cta-principal");
  if (cta) {
    if (os === "mac") cta.textContent = "Descargar para Mac";
  }

  var nav = document.querySelector(".nav");
  var onScroll = function () { if (nav) nav.classList.toggle("is-scrolled", window.scrollY > 8); };
  window.addEventListener("scroll", onScroll, { passive: true });
  onScroll();

  if ("IntersectionObserver" in window) {
    var items = document.querySelectorAll(".row, .lesson, .trio figure, .two, .dl, .title, .live");
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (entry) {
        if (entry.isIntersecting) { entry.target.classList.add("is-visible"); io.unobserve(entry.target); }
      });
    }, { rootMargin: "0px 0px -8% 0px" });
    items.forEach(function (el) { el.classList.add("reveal"); io.observe(el); });
  }
})();
