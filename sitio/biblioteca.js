/* Biblioteca: live list of the free Bible catalog and the backgrounds gallery. */
(function () {
  var list = document.getElementById("b-lista");
  if (!list) return;
  var search = document.getElementById("b-buscar"), lang = document.getElementById("b-idioma"), count = document.getElementById("b-cuenta");
  var all = [];

  function license(t) {
    var l = t.license || {};
    if (l.publicDomain) return "Dominio público";
    return l.commercialUse === "allowed" ? "Uso libre" : "Uso libre no comercial";
  }
  function norm(s) { return (s || "").toLowerCase().normalize("NFD").replace(/[̀-ͯ]/g, ""); }
  function draw() {
    var q = norm(search.value), l = lang.value, shown = 0, html = [];
    all.forEach(function (t) {
      if (l && t.language !== l) return;
      if (q && norm(t.name + " " + t.englishName + " " + t.shortName + " " + t.languageName).indexOf(q) < 0) return;
      if (++shown > 200) return;
      var books = t.numberOfBooks >= 66 ? "Biblia completa" : t.numberOfBooks === 27 ? "Nuevo Testamento" : t.numberOfBooks + " libros";
      html.push("<li><b>" + t.name.replace(/</g, "&lt;") + "</b><span>" + (t.shortName || t.id) + " · " + (t.languageName || t.language) + " · " + books + " · " + license(t) + "</span></li>");
    });
    list.innerHTML = html.join("");
    count.textContent = shown + (shown === 1 ? " traducción" : " traducciones") + (shown > 200 ? " (mostrando 200, refiná la búsqueda)" : "");
  }
  fetch("https://bible.helloao.org/api/available_translations.json").then(function (r) { return r.json(); }).then(function (data) {
    all = data.translations || [];
    var langs = {};
    all.forEach(function (t) { var k = t.language; langs[k] = langs[k] || { name: t.languageName || k, n: 0 }; langs[k].n++; });
    Object.keys(langs).sort(function (a, b) {
      if (a === "spa") return -1; if (b === "spa") return 1;
      return langs[a].name.localeCompare(langs[b].name);
    }).forEach(function (k) {
      var o = document.createElement("option"); o.value = k; o.textContent = langs[k].name + " (" + langs[k].n + ")"; lang.appendChild(o);
    });
    lang.value = "";
    draw();
  }).catch(function () { count.textContent = "No se pudo cargar el catálogo. Probá de nuevo más tarde."; });
  search.addEventListener("input", draw);
  lang.addEventListener("change", draw);

  /* Backgrounds: stills and animated loops (videos load when visible) */
  var grid = document.getElementById("f-grid");
  var stills = ["amanecer", "rayos-de-luz", "noche-estrellada", "aurora", "bokeh-dorado", "ondas-violeta"];
  var moving = ["aurora-viva", "brasas-suaves", "azul-profundo", "rosa-y-oro", "rayos-de-gloria", "luz-que-respira"];
  function title(n) { return (n.charAt(0).toUpperCase() + n.slice(1)).replace(/-/g, " "); }
  moving.forEach(function (n) {
    grid.insertAdjacentHTML("beforeend", '<figure><video muted loop playsinline preload="none" data-src="video/fondos/' + n + '.mp4" poster="img/fondos/' + n + '.jpg"></video><figcaption>' + title(n) + ' <em>animado</em></figcaption></figure>');
  });
  stills.forEach(function (n) {
    grid.insertAdjacentHTML("beforeend", '<figure><img src="img/fondos/' + n + '.jpg" alt="" loading="lazy"><figcaption>' + title(n) + '</figcaption></figure>');
  });
  var videos = grid.querySelectorAll("video");
  if ("IntersectionObserver" in window) {
    var io = new IntersectionObserver(function (entries) {
      entries.forEach(function (e) {
        var v = e.target;
        if (e.isIntersecting) { if (!v.src) v.src = v.dataset.src; v.play().catch(function () {}); } else v.pause();
      });
    }, { rootMargin: "100px" });
    [].forEach.call(videos, function (v) { io.observe(v); });
  }
})();
