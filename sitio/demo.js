/* Interactive WorshipStudio demo: content goes ON HOLD, "Pasar a pantalla"
 * sends it ON SCREEN. Everything is drawn with CSS; nothing is loaded. */
(function () {
  var demo = document.getElementById("demo");
  if (!demo) return;
  var $ = function (id) { return document.getElementById(id); };

  /* Window chrome like the visitor's system */
  var ua = navigator.userAgent || "";
  var platform = (navigator.userAgentData && navigator.userAgentData.platform) || navigator.platform || "";
  demo.dataset.os = /Win/i.test(platform + ua) ? "win" : /Linux|X11/i.test(platform + ua) && !/Android/i.test(ua) ? "linux" : "mac";

  /* Draw at 1200x700 and scale to the space available */
  var fit = demo.querySelector(".demo__fit"), app = demo.querySelector(".demo__app");
  function scale() { app.style.transform = "scale(" + fit.clientWidth / 1200 + ")"; }
  scale();
  window.addEventListener("resize", scale);

  /* Content (lyrics in the public domain, Reina-Valera 1909) */
  var songs = [
    { title: "Santo, santo, santo", bg: "bg-aurora", slides: ["¡Santo, santo, santo! Señor omnipotente,<br>siempre el labio mío loores te dará", "¡Santo, santo, santo! Te adoro reverente,<br>Dios en tres personas, bendita Trinidad"] },
    { title: "Castillo fuerte es nuestro Dios", bg: "bg-rosa", slides: ["Castillo fuerte es nuestro Dios,<br>defensa y buen escudo", "Con su poder nos librará<br>en este trance agudo"] },
    { title: "Cantad al Señor", bg: "bg-amanecer", slides: ["Cantad al Señor un cántico nuevo,<br>cantad al Señor toda la tierra", "Cantad al Señor, bendecid su nombre,<br>anunciad de día en día su salvación"] }
  ];
  var verses = [
    { ref: "Juan 3:16", bg: "bg-rayos", text: "Porque de tal manera amó Dios al mundo, que ha dado á su Hijo unigénito, para que todo aquel que en él cree, no se pierda, mas tenga vida eterna." },
    { ref: "Salmos 23:1", bg: "bg-amanecer", text: "Jehová es mi pastor; nada me faltará." },
    { ref: "Isaías 40:31", bg: "bg-noche", text: "Mas los que esperan á Jehová tendrán nuevas fuerzas; levantarán las alas como águilas." },
    { ref: "Salmos 46:10", bg: "bg-aurora", text: "Estad quietos, y conoced que yo soy Dios." }
  ];
  var hymns = [
    { n: 1, title: "Santo, santo, santo", song: 0 },
    { n: 2, title: "Castillo fuerte es nuestro Dios", song: 1 }
  ];
  var slides = [
    { label: "Bienvenidos", html: "Bienvenidos<small>Nos alegra que estés aquí</small>" },
    { label: "Anuncios", html: "Ensayo del coro<small>Sábado · 17:00</small>" },
    { label: "Ofrenda", html: "Gracias por tu ofrenda<small>Transferencias en el hall</small>" }
  ];
  var scenes = { camara: { bg: "bg-camara", html: "📷<small>Cámara principal</small>", label: "escena Cámara" },
                 pulpito: { bg: "bg-camara", html: "🎤<small>Púlpito</small>", label: "escena Púlpito" },
                 anuncios: { bg: "bg-anuncio", html: "Anuncios<small>WorshipStudio</small>", label: "escena Anuncios" } };

  /* An "item" is a list of slides with a name and a background */
  function songItem(i) { var s = songs[i]; return { name: "Cantos · " + s.title, bg: s.bg, slides: s.slides, at: 0 }; }
  function verseItem(i) { var v = verses[i]; return { name: "Biblia · " + v.ref, bg: v.bg, slides: [v.text + "<small>" + v.ref + " · Reina Valera 1909</small>"], at: 0 }; }
  function slideItem(i) { return { name: "Presentación · " + slides[i].label, bg: "bg-anuncio", slides: slides.map(function (s) { return s.html; }), at: i }; }
  function sceneItem(k) { var s = scenes[k]; return { name: s.label, bg: s.bg, slides: [s.html], at: 0, scene: k }; }

  var order = [
    { label: "Cuenta regresiva", make: function () { return { name: "Cuenta regresiva", bg: "bg-noche", slides: ["El culto comienza en<small>05:00</small>"], at: 0 }; } },
    { label: "Canto · Santo, santo, santo", make: function () { return songItem(0); } },
    { label: "Biblia · Salmos 23:1", make: function () { return verseItem(1); } },
    { label: "Anuncios", make: function () { return slideItem(1); } },
    { label: "Canto · Cantad al Señor", make: function () { return songItem(2); } }
  ];

  var waiting = null, live = sceneItem("camara"), hidden = false, studio = true, orderAt = -1;

  function paint(el, item, black) {
    var slide = el.querySelector(".d-slide");
    slide.className = "d-slide " + (item ? item.bg : "");
    slide.classList.toggle("is-black", !!black);
    slide.innerHTML = item ? "<div>" + item.slides[item.at] + "</div>" : "";
  }
  function render() {
    demo.classList.toggle("is-studio", studio);
    paint($("d-wait"), waiting);
    paint($("d-live"), live, hidden);
    $("d-st-live").textContent = live ? live.name + (live.slides.length > 1 ? " (" + (live.at + 1) + "/" + live.slides.length + ")" : "") : "nada";
    $("d-st-wait").textContent = waiting ? waiting.name : "nada";
    $("d-go").classList.toggle("pulse", !!waiting && studio);
    $("d-hide").classList.toggle("on", hidden);
    [].forEach.call(document.querySelectorAll("#d-scenes li"), function (li) {
      li.classList.toggle("on", !!live && live.scene === li.dataset.scene);
    });
    [].forEach.call(document.querySelectorAll("#d-order li"), function (li, i) {
      li.classList.toggle("on", i === orderAt); li.classList.toggle("done", i < orderAt);
    });
  }
  function hint() { var h = $("d-hint"); if (h) h.classList.add("gone"); }
  /* Choosing content: ON HOLD in studio mode, straight on screen otherwise */
  function choose(item) {
    hint();
    if (studio) waiting = item; else { live = item; hidden = false; }
    render();
  }
  function goLive() {
    if (!waiting) return;
    live = waiting; waiting = null; hidden = false; render();
  }
  function step(d) {
    if (!live || live.slides.length < 2) return;
    live.at = Math.max(0, Math.min(live.slides.length - 1, live.at + d)); render();
  }

  /* Lists */
  function fill(id, items, onPick) {
    var list = $(id);
    items.forEach(function (text, i) {
      var li = document.createElement("li"); li.textContent = text;
      li.addEventListener("click", function () {
        [].forEach.call(list.children, function (c) { c.classList.toggle("on", c === li); });
        onPick(i);
      });
      list.appendChild(li);
    });
  }
  fill("d-songs", songs.map(function (s) { return s.title; }), function (i) { choose(songItem(i)); thumbs(i); });
  fill("d-hymns", hymns.map(function (h) { return h.n + " · " + h.title; }), function (i) {
    var item = songItem(hymns[i].song); item.name = "Himnario · " + hymns[i].n + " " + hymns[i].title; choose(item);
  });
  fill("d-order", order.map(function (o) { return o.label; }), function (i) { orderAt = i; choose(order[i].make()); });
  function showVerses(filter) {
    var list = $("d-verses"); list.innerHTML = "";
    var f = (filter || "").toLowerCase().replace(/[áéíóú]/g, function (c) { return "aeiou"["áéíóú".indexOf(c)]; });
    verses.forEach(function (v, i) {
      var key = (v.ref + " " + v.text).toLowerCase().replace(/[áéíóú]/g, function (c) { return "aeiou"["áéíóú".indexOf(c)]; });
      if (f && key.indexOf(f.replace("sal ", "salmos ").replace("is ", "isaias ")) < 0 && key.indexOf(f) < 0) return;
      var li = document.createElement("li"); li.innerHTML = "<b>" + v.ref + "</b> " + v.text;
      li.addEventListener("click", function () { choose(verseItem(i)); });
      list.appendChild(li);
    });
  }
  showVerses("");
  $("d-bible").addEventListener("input", function (e) { showVerses(e.target.value); });
  function thumbs(i) {
    var box = $("d-thumbs"); box.innerHTML = "";
    songs[i].slides.forEach(function (text, k) {
      var b = document.createElement("button"); b.type = "button"; b.className = songs[i].bg; b.innerHTML = text.split("<br>")[0];
      b.addEventListener("click", function () { var item = songItem(i); item.at = k; choose(item); });
      box.appendChild(b);
    });
  }
  thumbs(0);
  slides.forEach(function (s, i) {
    var b = document.createElement("button"); b.type = "button"; b.className = "bg-anuncio"; b.textContent = s.label;
    b.addEventListener("click", function () { choose(slideItem(i)); });
    $("d-slides").appendChild(b);
  });

  /* Tabs */
  function tab(name) {
    [].forEach.call(document.querySelectorAll("#d-tabs button"), function (b) { b.classList.toggle("on", b.dataset.tab === name); });
    [].forEach.call(document.querySelectorAll(".d-panel"), function (p) { p.classList.toggle("on", p.dataset.panel === name); });
  }
  $("d-tabs").addEventListener("click", function (e) { if (e.target.dataset.tab) tab(e.target.dataset.tab); });
  tab("cantos");

  /* Scenes, live controls and the controls dock */
  $("d-scenes").addEventListener("click", function (e) { if (e.target.dataset.scene) choose(sceneItem(e.target.dataset.scene)); });
  $("d-go").addEventListener("click", goLive);
  $("d-go-mid").addEventListener("click", goLive);
  $("d-prev").addEventListener("click", function () { step(-1); });
  $("d-next").addEventListener("click", function () { step(1); });
  $("d-hide").addEventListener("click", function () { hidden = !hidden; render(); });
  $("d-next-item").addEventListener("click", function () {
    orderAt = Math.min(order.length - 1, orderAt + 1); choose(order[orderAt].make());
  });
  $("d-studio").addEventListener("click", function () {
    studio = !studio; this.classList.toggle("on", studio); if (!studio && waiting) { live = waiting; waiting = null; } render();
  });
  var started = 0, timer = null, title = demo.querySelector(".window__bar span");
  $("d-stream").addEventListener("click", function () {
    var on = !demo.classList.contains("is-live");
    demo.classList.toggle("is-live", on);
    this.classList.toggle("rec", on); this.textContent = on ? "Detener transmisión" : "Iniciar transmisión";
    clearInterval(timer);
    if (on) {
      started = Date.now();
      var tick = function () { var s = Math.floor((Date.now() - started) / 1000); title.dataset.time = ("0" + Math.floor(s / 60)).slice(-2) + ":" + ("0" + s % 60).slice(-2); };
      tick(); timer = setInterval(tick, 1000);
    }
  });
  $("d-record").addEventListener("click", function () {
    var on = !this.classList.contains("rec"); this.classList.toggle("rec", on); this.textContent = on ? "Detener grabación" : "Iniciar grabación";
  });

  /* Keyboard, like the real app: arrows advance, Enter sends to screen */
  demo.addEventListener("keydown", function (e) {
    if (e.target.tagName === "INPUT") return;
    if (e.key === "ArrowRight" || e.key === " ") { step(1); e.preventDefault(); }
    else if (e.key === "ArrowLeft") { step(-1); e.preventDefault(); }
    else if (e.key === "Enter") goLive();
    else if (e.key.toLowerCase() === "b") { hidden = !hidden; render(); }
  });

  render();
})();
