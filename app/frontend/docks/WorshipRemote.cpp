/******************************************************************************
    Copyright (C) 2026 by WorshipStudio Contributors

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "WorshipRemote.hpp"
#include "YouTubeConnect.hpp"
#include "WorshipLive.hpp"
#include "WorshipOutput.hpp"
#include "SongLibrary.hpp"
#include "AppearanceDialog.hpp"
#include "BackgroundLibrary.hpp"

#include <widgets/OBSBasic.hpp>

#include <QBuffer>

#include <obs-frontend-api.h>

#include <qrcodegen.hpp>
#include <qt-wrappers.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QImage>
#include <QHostAddress>
#include <QLocale>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QNetworkInterface>
#include <QPainter>
#include <QRandomGenerator>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>

#include "moc_WorshipRemote.cpp"

namespace {
constexpr const char *ConfigPin = "RemotePin";
constexpr qsizetype MaxRequest = 16 * 1024;

/* Texts of the phone page, translated to the interface language when the
 * page is served (the page itself is written in Spanish). */
const char *const PhoneTexts[] = {
	QT_TRANSLATE_NOOP("WorshipStudio", "Conectado"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Sin conexión"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Código de acceso"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Lo ves en el panel En vivo de WorshipStudio."),
	QT_TRANSLATE_NOOP("WorshipStudio", "Entrar"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Código incorrecto"),
	QT_TRANSLATE_NOOP("WorshipStudio", "EN ESPERA ·"),
	QT_TRANSLATE_NOOP("WorshipStudio", "EN PANTALLA ·"),
	QT_TRANSLATE_NOOP("WorshipStudio", "EN ESPERA"),
	QT_TRANSLATE_NOOP("WorshipStudio", "EN PANTALLA"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Pasar a pantalla ➜"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Pasar a la transmisión ➜"),
	QT_TRANSLATE_NOOP("WorshipStudio", "PROYECTOR"),
	QT_TRANSLATE_NOOP("WorshipStudio", "TRANSMISIÓN"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Proyector"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Cantos, Biblia..."),
	QT_TRANSLATE_NOOP("WorshipStudio", "Negro"),
	QT_TRANSLATE_NOOP("WorshipStudio", "negro"),
	QT_TRANSLATE_NOOP("WorshipStudio", "al aire"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Tocá una escena para cambiar la transmisión."),
	QT_TRANSLATE_NOOP("WorshipStudio", "AL AIRE"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Tocá una escena: queda en espera y la pasás con el botón verde."),
	QT_TRANSLATE_NOOP("WorshipStudio", "Transmisión"),
	QT_TRANSLATE_NOOP("WorshipStudio", "En vivo"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Escenas"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Contenido"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Nada en pantalla"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Deslizá ← → para cambiar de diapositiva"),
	QT_TRANSLATE_NOOP("WorshipStudio", "◀ Anterior"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Siguiente ▶"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Ocultar pantalla"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Mostrar pantalla"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Sigue"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Orden del culto"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Escenas"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Fuentes en pantalla"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Tocá para mostrar u ocultar. 🔇 silencia el audio."),
	QT_TRANSLATE_NOOP("WorshipStudio", "Himno"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Himnario"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Número, ej. 100"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Mostrar himno"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Biblia"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Ej. Juan 3:16 o Sal 23"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Mostrar pasaje"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Cantos"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Presentaciones"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Buscar canto"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Transmisión y grabación"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Transmitir"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Grabar"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Detener transmisión"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Detener grabación"),
	QT_TRANSLATE_NOOP("WorshipStudio", "¿Iniciar o detener la transmisión?"),
	QT_TRANSLATE_NOOP("WorshipStudio", "¿Iniciar o detener la grabación?"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Cuenta regresiva"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Mensaje al escenario"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Ej. 5 minutos"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Enviar"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Borrar"),
	QT_TRANSLATE_NOOP("WorshipStudio", "En vivo"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Más"),
	QT_TRANSLATE_NOOP("WorshipStudio", "No hay escenas propias todavía"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Visible"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Subir"),
	QT_TRANSLATE_NOOP("WorshipStudio", "+ Escena"),
	QT_TRANSLATE_NOOP("WorshipStudio", "+ Fuente"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Agregar a la escena"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Fondos"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Lo que ya tienes"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Cantos y coros"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Presentación"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Rótulo y mensaje"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Nada todavía"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Nombre de la escena nueva"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Fuentes en espera"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Los cambios quedan EN ESPERA hasta que tocas Pasar a pantalla."),
	QT_TRANSLATE_NOOP("WorshipStudio", "Toca para mostrar u ocultar; ▲ ▼ cambian el orden."),
	QT_TRANSLATE_NOOP("WorshipStudio", "Detener y volver a la escena"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Bajar"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Oculta"),
	QT_TRANSLATE_NOOP("WorshipStudio", "Sin fuentes"),
	QT_TRANSLATE_NOOP("WorshipStudio", "No hay cantos"),
	QT_TRANSLATE_NOOP("WorshipStudio", "nada"),
};

/* The phone page: no external files, works offline on the church network. */
const char *const RemotePage = R"HTML(<!doctype html>
<html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0E0F14"><title>WorshipStudio · Control</title>
<style>
:root{--bg:#070707;--s1:#0F0E0D;--s2:#181716;--line:#2B2825;--t:#F4F1EC;--m:#9C958B;--a:#DCC59C;--on:#16130E;--g:#E8C46A;--ok:#30A46C;--red:#E5484D;
/* Tipografia fluida: crece con el ancho, con tope arriba y abajo. */
--f-xs:clamp(10.5px,.55vw + 9.2px,12.5px);--f-sm:clamp(12px,.7vw + 10.1px,14px);--f-md:clamp(14px,.9vw + 11.6px,16.5px);--f-lg:clamp(16.5px,1.3vw + 12.5px,20px);--f-xl:clamp(19px,2vw + 13px,24px);--f-2xl:clamp(22px,3.4vw + 9px,32px);
/* Barra flotante de vidrio. */
--glass:rgba(22,20,18,.55);--glass-line:rgba(255,255,255,.10);--barH:calc(78px + env(safe-area-inset-bottom))}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;background:var(--bg);color:var(--t);font:var(--f-md)/1.45 -apple-system,system-ui,"Segoe UI",Roboto,sans-serif;padding:max(14px,env(safe-area-inset-top)) clamp(12px,3.5vw,20px) calc(var(--barH) + 18px);overflow-x:hidden}
header{display:flex;align-items:center;justify-content:space-between;margin-bottom:12px}
.brand{font-weight:700;letter-spacing:-.02em}.live{font-size:var(--f-sm);color:var(--m)}
.live b{display:inline-block;width:8px;height:8px;border-radius:50%;background:var(--ok);margin-right:6px}
.card{background:var(--s1);border:1px solid var(--line);border-radius:18px;padding:14px;margin-bottom:12px}
.k{font-size:var(--f-sm);letter-spacing:.1em;text-transform:uppercase;color:var(--m);margin-bottom:6px}
.onair{display:flex;gap:8px;align-items:center;font-size:var(--f-md);margin:2px 0}.dot{width:9px;height:9px;border-radius:50%;flex:none}
#title{font-weight:700;font-size:var(--f-lg)}#section{color:var(--g);font-size:var(--f-md);margin-top:2px}
#current{font-size:var(--f-xl);font-weight:600;white-space:pre-line;margin-top:8px;min-height:2.4em}
#next{color:var(--m);white-space:pre-line}
.nav{display:grid;grid-template-columns:1fr 1.6fr;gap:10px;margin-bottom:10px}
#title,#section{white-space:nowrap;overflow:hidden;text-overflow:ellipsis;min-height:1.3em}
.add{width:100%;margin-top:10px}
.sheet{position:fixed;inset:0;z-index:30;background:rgba(0,0,0,.55);display:none;align-items:flex-end;justify-content:center}.sheet.open{display:flex}
.sheetBox{background:var(--s1);width:100%;max-width:640px;max-height:86vh;overflow:auto;border:1px solid var(--line);border-radius:22px 22px 0 0;padding:16px 14px calc(18px + env(safe-area-inset-bottom));animation:up .18s ease-out}
@keyframes up{from{transform:translateY(40px);opacity:.5}}
.sheetHead{display:flex;justify-content:space-between;align-items:center;margin-bottom:12px;font-size:var(--f-lg)}
.libgrid{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin:6px 0 16px}.libgrid button{margin:0;padding:15px 8px;font-size:var(--f-md)}
.bggrid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin:6px 0 16px}.bggrid button{margin:0;padding:0;border-radius:12px;overflow:hidden;background:#000;aspect-ratio:16/9;position:relative}
.bggrid img{width:100%;height:100%;object-fit:cover;display:block}.bggrid span{position:absolute;left:6px;right:6px;bottom:4px;font-size:var(--f-xs);color:#fff;text-shadow:0 1px 3px #000;text-align:left;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
#current{height:5.4em;overflow:hidden;display:-webkit-box;-webkit-box-orient:vertical;-webkit-line-clamp:4}
.pill.ord{padding:9px 7px;min-width:32px}
button{font:600 var(--f-lg)/1.1 inherit;font-family:inherit;border:1px solid var(--line);border-radius:14px;padding:18px 10px;color:var(--t);background:var(--s2)}
button:active{transform:scale(.98)}.primary{background:var(--a);border-color:var(--a);color:var(--on);font-size:var(--f-xl)}
.go{width:100%;background:var(--ok);border-color:var(--ok);color:#fff;margin-bottom:10px}
.on{background:var(--g);color:#1A1407;border-color:var(--g)}.rec{background:var(--red);border-color:var(--red);color:#fff}
.list{list-style:none;margin:0;padding:0}.list li{display:flex;flex-wrap:wrap;align-items:center;gap:8px 10px;padding:13px 10px;border-radius:12px}
.list li+li{border-top:1px solid var(--line)}.list li.cur{background:var(--s2);font-weight:600}
.list li span.n{color:var(--m);font-size:var(--f-sm);min-width:22px;flex:none}
/* El nombre se recorta con puntos suspensivos en vez de partirse en tres
 * lineas, y si los botones no entran bajan solos a la linea de abajo. */
.list li .grow{flex:1 1 25%;min-width:0;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
.list li .pill,.list li .tag{flex:none}
.list li .acts{display:flex;align-items:center;gap:6px;margin-left:auto;flex:none}
.tag{font-size:var(--f-xs);font-weight:700;padding:3px 7px;border-radius:999px}.tag.p{background:var(--red)}.tag.w{background:var(--ok)}
.pill{font:600 var(--f-sm)/1 inherit;padding:9px 12px;border-radius:999px;border:1px solid var(--line);background:var(--s2);color:var(--t)}
.pill.off{color:var(--m);text-decoration:line-through}
input{width:100%;font:600 var(--f-xl)/1 inherit;padding:14px;border-radius:12px;border:1px solid var(--line);background:var(--s2);color:var(--t);margin:6px 0 10px}
.row2{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.tabs{position:fixed;z-index:20;left:50%;transform:translateX(-50%);bottom:calc(10px + env(safe-area-inset-bottom));width:min(520px,calc(100% - 20px));display:grid;grid-template-columns:repeat(4,1fr);gap:4px;padding:7px;border:1px solid var(--glass-line);border-radius:22px;background:var(--glass);backdrop-filter:blur(22px) saturate(160%);-webkit-backdrop-filter:blur(22px) saturate(160%);box-shadow:0 10px 30px rgba(0,0,0,.45),inset 0 1px 0 rgba(255,255,255,.06)}
/* Donde no hay desenfoque, un fondo solido para que se lea igual. */
@supports not ((backdrop-filter:blur(1px)) or (-webkit-backdrop-filter:blur(1px))){.tabs{background:rgba(22,20,18,.94)}}
.tabs a{text-align:center;color:var(--m);text-decoration:none;font-size:var(--f-xs);font-weight:600;padding:7px 2px;border-radius:16px;transition:background .15s,color .15s}
.tabs a.sel{color:var(--on);background:var(--a)}.tabs a i{display:block;font-style:normal;font-size:var(--f-xl);line-height:1.2}
.thumbs{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-bottom:12px}.thumbs figure{margin:0}.thumbs img{width:100%;aspect-ratio:16/9;object-fit:cover;border-radius:10px;background:#000;display:block;border:2px solid var(--line)}.thumbs .p img{border-color:var(--red)}.thumbs .w img{border-color:var(--ok)}.thumbs .w.pj img{border-color:#F2A65A}.chips{display:grid;grid-template-columns:1fr 1fr;gap:6px}.chips button{margin:0;padding:10px 8px;font-size:var(--f-md)}.chips button.on{background:#F2A65A;color:#1B1915;border-color:#F2A65A}.thumbs figcaption{font-size:var(--f-xs);font-weight:700;margin-top:4px;color:var(--m)}.swipe{touch-action:pan-y}.hintsw{font-size:var(--f-xs);color:var(--m);margin-top:6px}
section[data-tab]{display:none}section[data-tab].show{display:block}
/* YouTube: resultados con miniatura y botones de 10 en 10 segundos. */
.yt{display:flex;gap:10px;align-items:center;padding:10px 6px}.yt+.yt{border-top:1px solid var(--line)}
.yt img{width:clamp(96px,26vw,140px);aspect-ratio:16/9;object-fit:cover;border-radius:10px;background:#000;flex:none}
.yt .d{min-width:0}.yt .n{font-weight:600;display:-webkit-box;-webkit-box-orient:vertical;-webkit-line-clamp:2;overflow:hidden;font-size:var(--f-md)}
.yt .c{color:var(--m);font-size:var(--f-sm);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.ytrow{display:grid;grid-template-columns:1fr 1fr 1.4fr 1fr;gap:8px}.ytrow button{margin:0;padding:16px 6px}
@media(min-width:720px){body{max-width:1180px;margin:0 auto;padding-left:24px;padding-right:24px}main{display:block}section[data-tab].show{display:grid;gap:14px;align-items:start}section[data-tab="vivo"].show{grid-template-columns:1.3fr 1fr}section[data-tab="vivo"] .thumbs{grid-column:2;grid-row:1;margin:0}section[data-tab="vivo"] .thumbs img{border-radius:14px}.colA{grid-column:1;grid-row:1/span 3}#projCard{grid-column:2;grid-row:2;margin:0}#stThumbs,#goBtn2{grid-column:1/-1}#stThumbs{margin:0}.colB{grid-column:2;grid-row:3}.colB .card{margin:0 0 12px}section[data-tab="escenas"].show,section[data-tab="mas"].show{grid-template-columns:1fr 1fr}section[data-tab="contenido"].show{grid-template-columns:repeat(3,1fr)}section .card{margin:0}#current{font-size:var(--f-2xl)}.tabs{width:min(560px,70vw);bottom:16px}}
/* En tablet, dos columnas: tres aprietan demasiado los botones. */
@media(min-width:720px) and (max-width:1040px){section[data-tab="contenido"].show{grid-template-columns:1fr 1fr}}#login{display:none}.err{color:var(--red);font-size:var(--f-md);min-height:1.2em}
</style></head><body>
<header><div class="brand">WorshipStudio</div><div class="live" id="status"><b></b>Conectado</div></header>
<div id="login" class="card"><div class="k">Código de acceso</div>
<p style="margin:0;color:var(--m)">Lo ves en el panel En vivo de WorshipStudio.</p>
<input id="pin" inputmode="numeric" maxlength="4" autocomplete="one-time-code"><div class="err" id="err"></div>
<button class="primary" style="width:100%" onclick="savePin()">Entrar</button></div>
<main id="app">
<section data-tab="vivo" class="show">
<div class="thumbs"><figure class="w" id="waitRow"><img id="imgW" alt=""><figcaption><span id="lw">EN ESPERA</span> · <span id="prev"></span></figcaption></figure><figure class="p"><img id="imgP" alt=""><figcaption><span id="lp">EN PANTALLA</span> · <span id="prog"></span></figcaption></figure></div>
<div class="card" id="projCard" style="display:none"><div class="k">Proyector</div><div class="chips" id="proj"></div></div>
<div class="colA"><button class="go" id="goBtn" onclick="act('aire')">Pasar a pantalla ➜</button>
<div class="card swipe" id="cardNow"><div class="k" id="source">Contenido</div><div id="title">Nada en pantalla</div><div id="section"></div><div id="current"></div><div class="hintsw">Deslizá ← → para cambiar de diapositiva</div></div>
<div class="nav"><button onclick="act('anterior')">◀ Anterior</button><button class="primary" onclick="act('siguiente')">Siguiente ▶</button></div>
<button id="hide" style="width:100%" onclick="act('ocultar')">Ocultar pantalla</button></div><div class="colB">
<div class="card" style="margin-top:12px"><div class="k">Sigue</div><div id="next">—</div></div>
<div class="card" id="svc" style="display:none"><div class="k">Orden del culto</div><ol class="list" id="items"></ol></div></div>
</section>
<section data-tab="escenas">
<div class="thumbs" id="stThumbs" style="display:none"><figure class="w" id="stWait"><img id="imgSW" alt=""><figcaption>EN ESPERA · <span id="sprev"></span></figcaption></figure><figure class="p"><img id="imgSP" alt=""><figcaption>TRANSMISIÓN · <span id="sprog"></span></figcaption></figure></div>
<button class="go" id="goBtn2" style="display:none" onclick="act('aire')">Pasar a la transmisión ➜</button>
<div class="card"><div class="k">Escenas</div><p id="scHint" style="display:none;margin:0 0 8px;color:var(--m);font-size:var(--f-md)">Tocá una escena para cambiar la transmisión.</p><ul class="list" id="scenes"></ul><button class="add" onclick="newScene()">+ Escena</button></div>
<div class="card"><div class="k" id="srcK">Fuentes en pantalla</div><p id="srcHint" style="margin:0 0 8px;color:var(--m);font-size:var(--f-md)"></p><ul class="list" id="sources"></ul><button class="add primary" onclick="openLib()">+ Fuente</button></div>
</section>
<section data-tab="contenido">
<div class="card"><div class="k">Himno</div><input id="hymn" inputmode="numeric" placeholder="Número, ej. 100"><button class="primary" style="width:100%" onclick="act('himno',{n:val('hymn')})">Mostrar himno</button></div>
<div class="card"><div class="k">Biblia</div><input id="bible" placeholder="Ej. Juan 3:16 o Sal 23"><button class="primary" style="width:100%" onclick="act('biblia',{cita:val('bible')})">Mostrar pasaje</button></div>
<div class="card"><div class="k">Cantos</div><input id="songFilter" placeholder="Buscar canto" oninput="drawSongs()"><ul class="list" id="songs"></ul></div>
<div class="card"><div class="k">YouTube</div>
<input id="ytq" placeholder="Buscar un video o pegar el enlace" onkeydown="if(event.key==='Enter')ytSearch()">
<button class="primary" style="width:100%" onclick="ytSearch()">Buscar en YouTube</button>
<div id="ytr"></div>
<div id="ytnow" style="display:none;margin-top:12px">
<div class="k" style="margin-bottom:8px">En pantalla</div>
<div id="ytTitle" style="font-weight:600;margin-bottom:8px"></div>
<div class="ytrow"><button onclick="yt('restart')">↺</button><button onclick="ytSeek(-10)">− 10 s</button><button class="primary" id="ytPlay" onclick="ytPlayPause()">▶</button><button onclick="ytSeek(10)">+ 10 s</button></div>
<div class="hintsw" id="ytTime"></div></div></div>
</section>
<section data-tab="mas">
<div class="card"><div class="k">Transmisión y grabación</div><div class="row2"><button id="stream" onclick="confirmAct('transmision','¿Iniciar o detener la transmisión?')">Transmitir</button><button id="record" onclick="confirmAct('grabacion','¿Iniciar o detener la grabación?')">Grabar</button></div></div>
<div class="card"><div class="k">Cuenta regresiva</div><div class="row2" style="grid-template-columns:repeat(3,1fr)"><button onclick="act('cuenta',{min:3})">3 min</button><button onclick="act('cuenta',{min:5})">5 min</button><button onclick="act('cuenta',{min:10})">10 min</button></div><button class="add" onclick="act('detenercuenta')">Detener y volver a la escena</button></div>
<div class="card"><div class="k">Mensaje al escenario</div><input id="msg" placeholder="Ej. 5 minutos"><div class="row2"><button class="primary" onclick="act('mensaje',{texto:val('msg')})">Enviar</button><button onclick="document.getElementById('msg').value='';act('mensaje',{texto:''})">Borrar</button></div></div>
</section>
</main>
<div class="sheet" id="sheet" onclick="if(event.target===this)closeLib()"><div class="sheetBox"><div class="sheetHead"><b id="sheetT">Agregar a la escena</b><button class="pill" onclick="closeLib()">✕</button></div>
<div class="k">WorshipStudio</div><div class="libgrid" id="libWS"></div>
<div class="k">Fondos</div><div class="bggrid" id="libBg"></div>
<div class="k">Lo que ya tienes</div><ul class="list" id="libEx"></ul></div></div>
<nav class="tabs"><a href="#" data-go="vivo" class="sel"><i>▶︎</i><span id="tl1">En vivo</span></a><a href="#" data-go="escenas"><i>▦</i><span id="tl2">Escenas</span></a><a href="#" data-go="contenido"><i>♪</i>Contenido</a><a href="#" data-go="mas"><i>•••</i>Más</a></nav>
)HTML"
				      /* Two pieces: compilers limit the length of one literal. */
				      R"HTML(<script>
const T=/*T*/{},t=s=>T[s]||s;
(function(){const w=document.createTreeWalker(document.body,4);let n;while(n=w.nextNode()){const k=n.nodeValue.trim();if(T[k])n.nodeValue=n.nodeValue.replace(k,T[k]);}document.querySelectorAll("[placeholder]").forEach(e=>e.placeholder=t(e.placeholder));})();
let pin=localStorage.getItem('wsPin')||new URLSearchParams(location.search).get('pin')||'';
if(new URLSearchParams(location.search).get('pin')){localStorage.setItem('wsPin',pin);history.replaceState(null,'','/');}
let songs=[];const $=id=>document.getElementById(id);const esc=t=>String(t).replace(/[&<>"]/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;'}[c]));
const val=id=>$(id).value.trim();
document.querySelectorAll('.tabs a').forEach(a=>a.onclick=e=>{e.preventDefault();document.querySelectorAll('.tabs a').forEach(x=>x.classList.toggle('sel',x===a));document.querySelectorAll('section[data-tab]').forEach(s=>s.classList.toggle('show',s.dataset.tab===a.dataset.go));if(a.dataset.go==='contenido'&&!songs.length)loadSongs();});
function showLogin(msg){$('login').style.display='block';$('app').style.display='none';$('err').textContent=msg||'';}
function savePin(){pin=$('pin').value.trim();localStorage.setItem('wsPin',pin);$('login').style.display='none';$('app').style.display='block';poll();}
async function call(path,opts){const r=await fetch(path+(path.includes('?')?'&':'?')+'pin='+encodeURIComponent(pin),opts);if(r.status===401){showLogin(t('Código incorrecto'));throw 0;}return r.json();}
function friendly(n){if(!n)return t('nada');if(n.startsWith('WS ')){n=n.slice(3);if(n.endsWith(' B'))n=n.slice(0,-2);}return n;}
let studio=false,sep=false,busy=0;
function thumbs(){if(busy||document.hidden||!pin)return;const q='&t='+Date.now()+'&pin='+encodeURIComponent(pin),L=[];const vis=id=>$(id).offsetParent;
if(sep){if(vis('imgW'))L.push(['imgW','proyector']);if(vis('imgSP'))L.push(['imgSP','pantalla']);if(studio&&vis('imgSW'))L.push(['imgSW','espera']);}else if(vis('imgP')){L.push(['imgP','pantalla']);if(studio)L.push(['imgW','espera']);}
if(!L.length)return;busy=1;let left=L.length;const done=()=>{if(--left<=0)busy=0;};L.forEach(([id,c])=>{const i=new Image();i.onload=()=>{$(id).src=i.src;done();};i.onerror=done;i.src='/api/vista?cual='+c+q;});}
setInterval(thumbs,700);
let x0=null;$('cardNow').addEventListener('touchstart',e=>{x0=e.touches[0].clientX},{passive:true});
$('cardNow').addEventListener('touchend',e=>{if(x0===null)return;const dx=e.changedTouches[0].clientX-x0;x0=null;if(Math.abs(dx)>50)act(dx<0?'siguiente':'anterior');});
async function wake(){try{if('wakeLock' in navigator)await navigator.wakeLock.request('screen');}catch(e){}}
document.addEventListener('visibilitychange',()=>{if(!document.hidden)wake();});wake();
function render(s){if(s.accent){const r=document.documentElement.style,n=parseInt(s.accent.slice(1),16);r.setProperty('--a',s.accent);r.setProperty('--on',((n>>16)*299+(n>>8&255)*587+(n&255)*114)/255000>.6?'#1B1915':'#fff');}if(studio!==!!s.studio||sep!==!!s.sep){studio=!!s.studio;sep=!!s.sep;thumbs();}const two=studio||sep;$('prog').textContent=friendly(s.program);$('waitRow').style.display=two?'block':'none';$('waitRow').classList.toggle('pj',sep);$('imgP').parentNode.style.display=sep?'none':'block';$('waitRow').parentNode.style.gridTemplateColumns=(studio&&!sep)?'1fr 1fr':'1fr';$('goBtn').style.display=(s.studio&&!sep)?'block':'none';$('lw').textContent=t(sep?'PROYECTOR':'EN ESPERA');$('prev').textContent=sep?(s.proyector?friendly(s.proyector):t('negro')):friendly(s.preview);$('projCard').style.display=sep?'block':'none';$('scHint').style.display=sep?'block':'none';$('scHint').textContent=t(studio?'Tocá una escena: queda en espera y la pasás con el botón verde.':'Tocá una escena para cambiar la transmisión.');
$('stThumbs').style.display=sep?'grid':'none';$('stWait').style.display=studio?'block':'none';$('stThumbs').style.gridTemplateColumns=studio?'1fr 1fr':'1fr';$('goBtn2').style.display=(sep&&studio)?'block':'none';$('sprev').textContent=friendly(s.preview);$('sprog').textContent=friendly(s.program)+(s.streaming?' · '+t('al aire'):'');$('tl1').textContent=t(sep?'Proyector':'En vivo');$('tl2').textContent=t(sep?'Transmisión':'Escenas');
if(sep){const p=s.proyector||'',b=(k,l,on,dis)=>'<button class="'+(on?'on':'')+'"'+(dis?' disabled':'')+' onclick="act(\'proyector\',{nombre:'+JSON.stringify(k).replace(/"/g,'&quot;')+'})">'+esc(l)+'</button>';$('proj').innerHTML=b('__contenido',s.contenido?t(friendly(s.contenido)):t('Cantos, Biblia...'),p.startsWith('WS '),!s.contenido)+(s.proyectables||[]).map(n=>b(n,n,n===p)).join('')+b('',t('Negro'),!p);}
$('source').textContent=t(s.source||'Contenido');$('title').textContent=s.title||t('Nada en pantalla');
$('section').textContent=(s.section||'')+(s.count?'  ·  '+(s.index+1)+'/'+s.count:'');$('current').textContent=s.current||'';
$('next').textContent=s.next||'—';$('hide').classList.toggle('on',!!s.hidden);$('hide').textContent=t(s.hidden?'Mostrar pantalla':'Ocultar pantalla');
const it=s.service||[];$('svc').style.display=it.length?'block':'none';
$('items').innerHTML=it.map((t,i)=>'<li class="'+(i===s.serviceIndex?'cur':'')+'" onclick="act(\'item\',{n:'+i+'})"><span class="n">'+(i+1)+'</span><span class="grow">'+esc(t)+'</span></li>').join('');
$('scenes').innerHTML=(s.scenes||[]).map(n=>'<li class="'+(n===s.program?'cur':'')+'" onclick="act(\'escena\',{nombre:'+JSON.stringify(n).replace(/"/g,'&quot;')+'})"><span class="grow">'+esc(n)+'</span>'+(n===s.program?'<span class="tag p">'+t(sep?'AL AIRE':'EN PANTALLA')+'</span>':n===s.preview?'<span class="tag w">'+t('EN ESPERA')+'</span>':'')+'</li>').join('')||'<li>'+t('No hay escenas propias todavía')+'</li>';
$('sources').innerHTML=(s.sources||[]).map(f=>{const n=JSON.stringify(f.nombre).replace(/"/g,'&quot;');return '<li><span class="grow" title="'+esc(friendly(f.nombre))+'">'+esc(friendly(f.nombre))+'</span><span class="acts">'+(f.audio?'<button class="pill ord'+(f.silencio?' off':'')+'" onclick="act(\'silencio\',{nombre:'+n+'})">'+(f.silencio?'🔇':'🔊')+'</button><button class="pill ord'+(f.auriculares?' on':'')+'" aria-label="'+t('Auriculares')+'" onclick="act(\'auriculares\',{nombre:'+n+'})">🎧</button>':'')+'<button class="pill'+(f.visible?'':' off')+'" onclick="act(\'fuente\',{nombre:'+n+'})">'+t(f.visible?'Visible':'Oculta')+'</button><button class="pill ord" aria-label="'+t('Subir')+'" onclick="act(\'subir\',{nombre:'+n+'})">▲</button><button class="pill ord" aria-label="'+t('Bajar')+'" onclick="act(\'bajar\',{nombre:'+n+'})">▼</button></span></li>';}).join('')||'<li>'+t('Sin fuentes')+'</li>';
$('srcK').textContent=t(studio?'Fuentes en espera':'Fuentes en pantalla');$('srcHint').textContent=t(studio?'Los cambios quedan EN ESPERA hasta que tocas Pasar a pantalla.':'Toca para mostrar u ocultar; 🎧 es escucharla en los auriculares; ▲ ▼ cambian el orden.');
$('stream').classList.toggle('rec',!!s.streaming);$('stream').textContent=t(s.streaming?'Detener transmisión':'Transmitir');
$('record').classList.toggle('rec',!!s.recording);$('record').textContent=t(s.recording?'Detener grabación':'Grabar');
const y=s.youtube;$('ytnow').style.display=y?'block':'none';
if(y){$('ytTitle').textContent=y.titulo||'YouTube';ytPlaying=y.estado===1;$('ytPlay').textContent=ytPlaying?'⏸':'▶';
$('ytTime').textContent=y.duracion?ytClock(y.tiempo)+' / '+ytClock(y.duracion):t('Listo para reproducir');}
$('status').innerHTML='<b></b>'+t('Conectado');}
async function act(a,params){if(navigator.vibrate)navigator.vibrate(12);const q=Object.entries(params||{}).map(([k,v])=>'&'+k+'='+encodeURIComponent(v)).join('');try{render(await call('/api/accion?a='+a+q,{method:'POST'}));}catch(e){}}
let lib=null;
async function openLib(){$('sheet').classList.add('open');try{lib=await call('/api/biblioteca');drawLib();}catch(e){}}
function closeLib(){$('sheet').classList.remove('open');}
function addSrc(k,v){closeLib();act('agregar',{tipo:k,valor:v});}
function drawLib(){const ws=[['panel','songsDock','Cantos y coros'],['panel','bibleDock','Biblia'],['panel','hymnalDock','Himnario'],['panel','presentationDock','Presentación'],['graficos','','Rótulo y mensaje'],['cuenta','','Cuenta regresiva']];
$('libWS').innerHTML=ws.map(w=>'<button onclick="addSrc(\''+w[0]+'\',\''+w[1]+'\')">'+esc(t(w[2]))+'</button>').join('');
const q='&pin='+encodeURIComponent(pin);$('libBg').innerHTML=(lib.fondos||[]).map((f,i)=>'<button onclick="addSrc(\'fondo\',\''+i+'\')"><img loading="lazy" alt="" src="/api/miniatura?n='+i+q+'"><span>'+(f.video?'▶ ':'')+esc(f.nombre)+'</span></button>').join('');
$('libEx').innerHTML=(lib.existentes||[]).map(n=>'<li onclick="addSrc(\'existente\','+JSON.stringify(n).replace(/"/g,'&quot;')+')"><span class="grow">'+esc(n)+'</span><span class="pill">+</span></li>').join('')||'<li>'+esc(t('Nada todavía'))+'</li>';}
function newScene(){const n=prompt(t('Nombre de la escena nueva'),'');if(n&&n.trim())act('nuevaescena',{nombre:n.trim()});}
function confirmAct(a,text){if(confirm(t(text)))act(a);}
let ytPlaying=false;
async function ytSearch(){const q=val('ytq');if(!q)return;const link=(q.match(/(?:v=|youtu\.be\/|\/embed\/|\/live\/|\/shorts\/)([A-Za-z0-9_-]{11})/)||(q.match(/^[A-Za-z0-9_-]{11}$/)?[0,q]:null));
if(link){ytShow(link[1],t('Video de YouTube'));$('ytr').innerHTML='';return;}
$('ytr').innerHTML='<p style="color:var(--m)">'+t('Buscando en YouTube...')+'</p>';
try{const r=await call('/api/youtube?q='+encodeURIComponent(q));const v=r.videos||[];
$('ytr').innerHTML=v.length?v.map(x=>'<div class="yt" onclick=\'ytShow('+JSON.stringify(x.id)+','+JSON.stringify(x.titulo)+')\'><img loading="lazy" alt="" src="https://i.ytimg.com/vi/'+esc(x.id)+'/mqdefault.jpg"><div class="d"><div class="n">'+esc(x.titulo)+'</div><div class="c">'+esc(x.canal||'')+(x.duracion?' · '+esc(x.duracion):'')+'</div></div></div>').join(''):'<p style="color:var(--m)">'+t('Sin resultados. Prueba con otras palabras o pega el enlace del video.')+'</p>';}
catch(e){$('ytr').innerHTML='<p style="color:var(--red)">'+t('No se pudo buscar. Revisa la conexión a internet.')+'</p>';}}
function ytShow(id,titulo){act('youtube',{v:id,titulo:titulo});ytPlaying=false;}
function yt(cmd){act('ytcontrol',{cmd:cmd});if(cmd==='play'||cmd==='restart')ytPlaying=true;if(cmd==='pause')ytPlaying=false;}
function ytSeek(seg){act('ytcontrol',{cmd:'seek',seg:seg});}
function ytPlayPause(){yt(ytPlaying?'pause':'play');}
function ytClock(s){s=Math.max(0,Math.round(s||0));const h=Math.floor(s/3600),m=Math.floor(s/60)%60,x=s%60;const p=n=>(n<10?'0':'')+n;return h?h+':'+p(m)+':'+p(x):m+':'+p(x);}
async function loadSongs(){try{songs=await call('/api/cantos');drawSongs();}catch(e){}}
function drawSongs(){const f=val('songFilter').toLowerCase();$('songs').innerHTML=songs.filter(s=>s.titulo.toLowerCase().includes(f)).map(s=>'<li onclick="act(\'canto\',{id:\''+s.id+'\'})"><span class="grow">'+esc(s.titulo)+'</span><span class="n">▶︎</span></li>').join('')||'<li>'+t('No hay cantos')+'</li>';}
async function poll(){try{render(await call('/api/estado'));}catch(e){if(e!==0)$('status').innerHTML='<b style="background:var(--red)"></b>'+t('Sin conexión');}}
if(!pin)showLogin();else poll();setInterval(()=>{if(pin)poll();},1000);
</script></body></html>)HTML";

QString localAddress()
{
	/* Prefer private LAN addresses (192.168.x, 10.x, 172.16-31.x). */
	QString fallback;
	for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
		if (!(iface.flags() & QNetworkInterface::IsUp) || (iface.flags() & QNetworkInterface::IsLoopBack))
			continue;
		for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
			const QHostAddress ip = entry.ip();
			if (ip.protocol() != QAbstractSocket::IPv4Protocol)
				continue;
			/* 10.x, 172.16-31.x and 192.168.x (Qt 6.4 has no isPrivateUse). */
			const bool privateLan = ip.isInSubnet(QHostAddress(QStringLiteral("10.0.0.0")), 8) ||
						ip.isInSubnet(QHostAddress(QStringLiteral("172.16.0.0")), 12) ||
						ip.isInSubnet(QHostAddress(QStringLiteral("192.168.0.0")), 16);
			if (privateLan)
				return ip.toString();
			if (fallback.isEmpty())
				fallback = ip.toString();
		}
	}
	return fallback.isEmpty() ? QStringLiteral("127.0.0.1") : fallback;
}
} // namespace

WorshipRemote::WorshipRemote(QObject *parent) : QObject(parent)
{
	accessPin = WorshipOutput::ConfigString(ConfigPin);
	if (accessPin.size() != 4)
		regeneratePin();
}

WorshipRemote::~WorshipRemote()
{
	stop();
}

void WorshipRemote::regeneratePin()
{
	accessPin = QStringLiteral("%1").arg(QRandomGenerator::global()->bounded(10000), 4, 10, QLatin1Char('0'));
	WorshipOutput::SetConfigString(ConfigPin, accessPin);
}

bool WorshipRemote::start(quint16 port)
{
	if (server)
		return true;
	server = new QTcpServer(this);
	/* Try a few ports in case another program uses the first one. */
	for (quint16 candidate = port; candidate < port + 10; ++candidate) {
		if (server->listen(QHostAddress::AnyIPv4, candidate)) {
			listenPort = candidate;
			connect(server, &QTcpServer::newConnection, this, &WorshipRemote::onConnection);
			blog(LOG_INFO, "[WorshipStudio] Control remoto en %s", QT_TO_UTF8(url()));
			emit runningChanged(true);
			return true;
		}
	}
	delete server;
	server = nullptr;
	return false;
}

void WorshipRemote::stop()
{
	if (!server)
		return;
	server->close();
	server->deleteLater();
	server = nullptr;
	emit runningChanged(false);
}

bool WorshipRemote::isRunning() const
{
	return server && server->isListening();
}

QString WorshipRemote::url() const
{
	return QStringLiteral("http://%1:%2").arg(localAddress()).arg(listenPort);
}

QImage WorshipRemote::qrCode(int size) const
{
	const std::string text = QStringLiteral("%1/?pin=%2").arg(url(), accessPin).toStdString();
	const qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText(text.c_str(), qrcodegen::QrCode::Ecc::MEDIUM);
	const int modules = qr.getSize() + 8; /* quiet zone of 4 modules */
	QImage image(size, size, QImage::Format_RGB32);
	image.fill(Qt::white);
	QPainter painter(&image);
	painter.setPen(Qt::NoPen);
	painter.setBrush(Qt::black);
	const qreal cell = (qreal)size / modules;
	for (int y = 0; y < qr.getSize(); ++y) {
		for (int x = 0; x < qr.getSize(); ++x) {
			if (qr.getModule(x, y))
				painter.drawRect(QRectF((x + 4) * cell, (y + 4) * cell, cell + 0.5, cell + 0.5));
		}
	}
	return image;
}

void WorshipRemote::onConnection()
{
	while (server && server->hasPendingConnections()) {
		QTcpSocket *socket = server->nextPendingConnection();
		connect(socket, &QTcpSocket::readyRead, this, [this, socket]() { onReadyRead(socket); });
		connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
	}
}

void WorshipRemote::onReadyRead(QTcpSocket *socket)
{
	QByteArray buffer = socket->property("buffer").toByteArray() + socket->readAll();
	if (buffer.size() > MaxRequest) {
		socket->disconnectFromHost();
		return;
	}
	const qsizetype headerEnd = buffer.indexOf("\r\n\r\n");
	if (headerEnd < 0) {
		socket->setProperty("buffer", buffer);
		return;
	}

	const QList<QByteArray> requestLine = buffer.left(buffer.indexOf("\r\n")).split(' ');
	if (requestLine.size() < 2) {
		socket->disconnectFromHost();
		return;
	}

	const QString target = QString::fromUtf8(requestLine.at(1));
	/* Searching on YouTube takes a moment: that answer leaves later. */
	if (deferYouTubeSearch(socket, target))
		return;

	int status = 200;
	QByteArray contentType;
	const QByteArray body = respond(requestLine.at(0), target, &status, &contentType);
	send(socket, status, contentType, body);
}

void WorshipRemote::send(QTcpSocket *socket, int status, const QByteArray &contentType, const QByteArray &body)
{
	const QByteArray reason = status == 200 ? "OK" : status == 401 ? "Unauthorized" : "Not Found";
	QByteArray response = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason + "\r\n";
	response += "Content-Type: " + contentType + "\r\n";
	response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
	response += "Cache-Control: no-store\r\nConnection: close\r\n\r\n";
	response += body;
	socket->write(response);
	socket->disconnectFromHost();
}

/* The search on YouTube goes out to the internet: the page asks, the app
 * answers when the results come back. */
bool WorshipRemote::deferYouTubeSearch(QTcpSocket *socket, const QString &target)
{
	const QUrl url(target);
	if (url.path() != QStringLiteral("/api/youtube"))
		return false;
	const QUrlQuery query(url);
	const QByteArray json = "application/json; charset=utf-8";
	if (query.queryItemValue(QStringLiteral("pin")) != accessPin) {
		send(socket, 401, json, R"({"error":"pin"})");
		return true;
	}
	QPointer<QTcpSocket> alive(socket);
	YouTubeConnect::SearchVideos(this, query.queryItemValue(QStringLiteral("q"), QUrl::FullyDecoded),
				     [this, alive, json](const QJsonArray &videos) {
					     if (!alive)
						     return;
					     send(alive, 200, json,
						  QJsonDocument(QJsonObject{{QStringLiteral("videos"), videos}})
							  .toJson(QJsonDocument::Compact));
				     });
	return true;
}

namespace {
bool userScene(const QString &name);
QByteArray backgroundThumbnail(const QString &path, bool video);
} // namespace

QByteArray WorshipRemote::handleForTest(const QByteArray &method, const QString &target)
{
	int status = 0;
	QByteArray type;
	return respond(method, target, &status, &type);
}

QByteArray WorshipRemote::respond(const QByteArray &method, const QString &target, int *status,
				  QByteArray *contentType)
{
	const QUrl url(target);
	const QUrlQuery query(url);
	const QString path = url.path();

	if (path == QStringLiteral("/") || path == QStringLiteral("/index.html")) {
		*contentType = "text/html; charset=utf-8";
		QJsonObject texts;
		for (const char *text : PhoneTexts) {
			const QString translated = QCoreApplication::translate("WorshipStudio", text);
			if (translated != QString::fromUtf8(text))
				texts.insert(QString::fromUtf8(text), translated);
		}
		QByteArray page(RemotePage);
		page.replace("/*T*/{}", QJsonDocument(texts).toJson(QJsonDocument::Compact));
		page.replace("<html lang=\"es\">", "<html lang=\"" + QLocale().name().section(QLatin1Char('_'), 0, 0).toUtf8() + "\">");
		return page;
	}

	*contentType = "application/json; charset=utf-8";
	if (!path.startsWith(QStringLiteral("/api/"))) {
		*status = 404;
		return "{}";
	}
	if (query.queryItemValue(QStringLiteral("pin")) != accessPin) {
		*status = 401;
		return R"({"error":"pin"})";
	}

	if (path == QStringLiteral("/api/accion") && method == "POST") {
		const QString action = query.queryItemValue(QStringLiteral("a"));
		WorshipLive &live = WorshipLive::instance();
		/* Run on the UI thread; the state is refreshed by the next poll. */
		const QString name = query.queryItemValue(QStringLiteral("nombre"), QUrl::FullyDecoded);
		if (action == QStringLiteral("siguiente"))
			live.next();
		else if (action == QStringLiteral("anterior"))
			live.previous();
		else if (action == QStringLiteral("ocultar"))
			live.toggleHidden();
		else if (action == QStringLiteral("item"))
			live.requestServiceItem(query.queryItemValue(QStringLiteral("n")).toInt());
		else if (action == QStringLiteral("aire") && obs_frontend_preview_program_mode_active())
			obs_frontend_preview_program_trigger_transition();
		else if (action == QStringLiteral("escena"))
			showScene(name);
		else if (action == QStringLiteral("proyector")) {
			/* "__contenido": the last song, verse, hymn or presentation. */
			const QString scene = name == QStringLiteral("__contenido") ? WorshipOutput::LastContentScene() : name;
			OBSSourceAutoRelease source = scene.isEmpty() ? nullptr : obs_get_source_by_name(QT_TO_UTF8(scene));
			if (scene.isEmpty() || source)
				WorshipOutput::ShowOnProjector(source);
		}
		else if (action == QStringLiteral("fuente"))
			toggleSource(name);
		else if (action == QStringLiteral("subir") || action == QStringLiteral("bajar"))
			moveSource(name, action == QStringLiteral("subir"));
		else if (action == QStringLiteral("agregar"))
			addSource(query.queryItemValue(QStringLiteral("tipo")),
				  query.queryItemValue(QStringLiteral("valor"), QUrl::FullyDecoded));
		else if (action == QStringLiteral("nuevaescena"))
			addScene(name);
		else if (action == QStringLiteral("detenercuenta"))
			live.requestCountdownStop();
		else if (action == QStringLiteral("silencio"))
			toggleMute(name);
		else if (action == QStringLiteral("auriculares"))
			toggleMonitor(name);
		else if (action == QStringLiteral("himno"))
			live.requestHymn(query.queryItemValue(QStringLiteral("n")).toInt());
		else if (action == QStringLiteral("biblia"))
			live.requestBible(query.queryItemValue(QStringLiteral("cita"), QUrl::FullyDecoded));
		else if (action == QStringLiteral("youtube"))
			YouTubeConnect::Show(query.queryItemValue(QStringLiteral("v")),
					     query.queryItemValue(QStringLiteral("titulo"), QUrl::FullyDecoded));
		else if (action == QStringLiteral("ytcontrol")) {
			const QString cmd = query.queryItemValue(QStringLiteral("cmd"));
			if (cmd == QStringLiteral("play") || cmd == QStringLiteral("pause") ||
			    cmd == QStringLiteral("restart"))
				YouTubeConnect::Command(cmd);
			else if (cmd == QStringLiteral("seek"))
				YouTubeConnect::Command(cmd,
							qBound(-600, query.queryItemValue(QStringLiteral("seg")).toInt(), 600));
		}
		else if (action == QStringLiteral("canto"))
			live.requestSong(query.queryItemValue(QStringLiteral("id")));
		else if (action == QStringLiteral("mensaje"))
			live.setStageMessage(query.queryItemValue(QStringLiteral("texto"), QUrl::FullyDecoded).left(140));
		else if (action == QStringLiteral("cuenta"))
			live.requestCountdown(qBound(1, query.queryItemValue(QStringLiteral("min")).toInt(), 240));
		else if (action == QStringLiteral("transmision"))
			obs_frontend_streaming_active() ? obs_frontend_streaming_stop() : obs_frontend_streaming_start();
		else if (action == QStringLiteral("grabacion"))
			obs_frontend_recording_active() ? obs_frontend_recording_stop() : obs_frontend_recording_start();
	}
	if (path == QStringLiteral("/api/biblioteca")) {
		/* What "+ Fuente" offers: backgrounds and the sources already made. */
		QJsonArray backgrounds;
		for (const BackgroundLibrary::Item &item : BackgroundLibrary::Items(true))
			backgrounds.append(QJsonObject{{QStringLiteral("nombre"), item.name}, {QStringLiteral("video"), item.video}});
		QStringList existing;
		obs_enum_sources(
			[](void *data, obs_source_t *source) {
				const QString name = QT_UTF8(obs_source_get_name(source));
				if (!obs_source_is_hidden(source) && !name.startsWith(QStringLiteral("WS ")))
					static_cast<QStringList *>(data)->append(name);
				return true;
			},
			&existing);
		char **names = obs_frontend_get_scene_names();
		for (char **name = names; name && *name; ++name) {
			if (userScene(QT_UTF8(*name)))
				existing.append(QT_UTF8(*name));
		}
		bfree(names);
		existing.sort(Qt::CaseInsensitive);
		return QJsonDocument(QJsonObject{{QStringLiteral("fondos"), backgrounds},
						 {QStringLiteral("existentes"), QJsonArray::fromStringList(existing)}})
			.toJson(QJsonDocument::Compact);
	}
	if (path == QStringLiteral("/api/miniatura")) {
		const QVector<BackgroundLibrary::Item> items = BackgroundLibrary::Items(true);
		const int index = query.queryItemValue(QStringLiteral("n")).toInt();
		if (index < 0 || index >= items.size()) {
			*status = 404;
			return "{}";
		}
		*contentType = "image/jpeg";
		return backgroundThumbnail(items.at(index).path, items.at(index).video);
	}
	if (path == QStringLiteral("/api/vista")) {
		/* Small live picture of the screen or the preview for the phone. */
		const bool preview = query.queryItemValue(QStringLiteral("cual")) == QStringLiteral("espera");
		OBSSourceAutoRelease scene = preview ? (obs_frontend_preview_program_mode_active()
								? obs_frontend_get_current_preview_scene()
								: nullptr)
						     : query.queryItemValue(QStringLiteral("cual")) == QStringLiteral("proyector")
							       ? obs_get_source_by_name(WorshipOutput::RoomScreenScene)
							       : obs_get_output_source(0);
		QImage image = WorshipOutput::RenderSource(scene, 480, 270);
		if (image.isNull()) {
			image = QImage(480, 270, QImage::Format_RGB32);
			image.fill(Qt::black);
		}
		QByteArray jpeg;
		QBuffer buffer(&jpeg);
		buffer.open(QIODevice::WriteOnly);
		image.convertToFormat(QImage::Format_RGB32).save(&buffer, "JPG", 70);
		*contentType = "image/jpeg";
		return jpeg;
	}
	if (path == QStringLiteral("/api/cantos")) {
		QJsonArray songs;
		for (const Song &song : SongLibrary::loadAll())
			songs.append(QJsonObject{{QStringLiteral("id"), song.id}, {QStringLiteral("titulo"), song.title}});
		return QJsonDocument(songs).toJson(QJsonDocument::Compact);
	}
	return stateJson();
}

namespace {
/* Scenes the phone can switch to: the user's own (WorshipStudio scenes are
 * driven by the content tabs). */
bool userScene(const QString &name)
{
	return !WorshipOutput::IsInternalScene(name);
}

obs_source_t *programSource()
{
	return obs_frontend_get_current_scene();
}

/* The scene the phone edits: in Studio Mode the one waiting in the preview
 * (changes go to the screen with "Pasar a pantalla"), otherwise the one on
 * screen. Like the PC. */
obs_source_t *editedSource()
{
	return obs_frontend_preview_program_mode_active() ? obs_frontend_get_current_preview_scene()
							  : obs_frontend_get_current_scene();
}

obs_sceneitem_t *editedItem(obs_source_t *edited, const QString &name)
{
	obs_scene_t *scene = edited ? obs_scene_from_source(edited) : nullptr;
	return scene ? obs_scene_find_source(scene, QT_TO_UTF8(name)) : nullptr;
}

/* Thumbnails of Mis fondos for the phone, made once. */
QByteArray backgroundThumbnail(const QString &path, bool video)
{
	static QHash<QString, QByteArray> cache;
	if (cache.contains(path))
		return cache.value(path);
	QImage image;
	if (video) {
		const QFileInfo info(path);
		image.load(info.absoluteDir().absoluteFilePath(QStringLiteral("miniaturas/%1.jpg").arg(info.completeBaseName())));
	} else {
		image.load(path);
	}
	if (image.isNull()) {
		image = QImage(240, 135, QImage::Format_RGB32);
		image.fill(QColor(0x22, 0x20, 0x1D));
	}
	image = image.scaled(240, 135, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation)
			.copy(QRect(0, 0, 240, 135));
	QByteArray jpeg;
	QBuffer buffer(&jpeg);
	buffer.open(QIODevice::WriteOnly);
	image.convertToFormat(QImage::Format_RGB32).save(&buffer, "JPG", 75);
	cache.insert(path, jpeg);
	return jpeg;
}
} // namespace

void WorshipRemote::showScene(const QString &name)
{
	OBSSourceAutoRelease source = obs_get_source_by_name(QT_TO_UTF8(name));
	if (!source || !obs_scene_from_source(source) || !userScene(name))
		return;
	if (obs_frontend_preview_program_mode_active())
		obs_frontend_set_current_preview_scene(source);
	else
		obs_frontend_set_current_scene(source);
}

void WorshipRemote::toggleSource(const QString &name)
{
	OBSSourceAutoRelease edited = editedSource();
	if (obs_sceneitem_t *item = editedItem(edited, name))
		obs_sceneitem_set_visible(item, !obs_sceneitem_visible(item));
}

void WorshipRemote::moveSource(const QString &name, bool up)
{
	OBSSourceAutoRelease edited = editedSource();
	if (obs_sceneitem_t *item = editedItem(edited, name))
		obs_sceneitem_set_order(item, up ? OBS_ORDER_MOVE_UP : OBS_ORDER_MOVE_DOWN);
}

/* "+ Fuente" on the phone: WorshipStudio's library, a background or a source
 * the user already has, into the scene being edited. */
void WorshipRemote::addSource(const QString &kind, const QString &value)
{
	OBSBasic *main = OBSBasic::Get();
	if (!main)
		return;
	if (kind == QStringLiteral("panel")) {
		main->AddPanelSource(value, false);
	} else if (kind == QStringLiteral("graficos") || kind == QStringLiteral("cuenta")) {
		main->AddGraphicsSource(kind == QStringLiteral("cuenta"));
	} else if (kind == QStringLiteral("fondo")) {
		const QVector<BackgroundLibrary::Item> items = BackgroundLibrary::Items(true);
		const int index = value.toInt();
		if (index >= 0 && index < items.size())
			main->AddBackgroundSource(items.at(index).path, items.at(index).name);
	} else if (kind == QStringLiteral("existente")) {
		OBSSourceAutoRelease edited = editedSource();
		obs_source_t *target = edited;
		OBSSource user;
		if (target && WorshipOutput::IsInternalScene(QT_UTF8(obs_source_get_name(target)))) {
			user = WorshipOutput::LastUserScene();
			target = user;
		}
		obs_scene_t *scene = target ? obs_scene_from_source(target) : nullptr;
		OBSSourceAutoRelease source = obs_get_source_by_name(QT_TO_UTF8(value));
		if (scene && source && source.Get() != target && !obs_scene_find_source(scene, QT_TO_UTF8(value)))
			obs_scene_add(scene, source);
	}
}

/* "+ Escena": a new empty scene; in Studio Mode it waits in the preview. */
void WorshipRemote::addScene(const QString &requested)
{
	const QString base = requested.trimmed().left(60);
	if (base.isEmpty())
		return;
	QString name = base;
	for (int i = 2;; ++i) {
		OBSSourceAutoRelease taken = obs_get_source_by_name(QT_TO_UTF8(name));
		if (!taken)
			break;
		name = QStringLiteral("%1 %2").arg(base).arg(i);
	}
	OBSSceneAutoRelease scene = obs_scene_create(QT_TO_UTF8(name));
	if (scene && obs_frontend_preview_program_mode_active())
		obs_frontend_set_current_preview_scene(obs_scene_get_source(scene));
}

/* The speaker and the headphones work together, like the mixer of the PC:
 * the source is really muted only when the headphones are off. */
void WorshipRemote::toggleMute(const QString &name)
{
	OBSSourceAutoRelease source = obs_get_source_by_name(QT_TO_UTF8(name));
	if (!source || !(obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO))
		return;
	const obs_monitoring_type monitoring = obs_source_get_monitoring_type(source);
	const bool mute = !(obs_source_muted(source) || monitoring == OBS_MONITORING_TYPE_MONITOR_ONLY);

	if (mute && monitoring == OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT)
		obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_MONITOR_ONLY);
	else if (!mute && monitoring == OBS_MONITORING_TYPE_MONITOR_ONLY)
		obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
	else
		obs_source_set_muted(source, mute);
}

/* Headphones: hear that source in the headphones of the operator, with or
 * without sending it to the transmission. */
void WorshipRemote::toggleMonitor(const QString &name)
{
	OBSSourceAutoRelease source = obs_get_source_by_name(QT_TO_UTF8(name));
	if (!source || !(obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO))
		return;
	const obs_monitoring_type monitoring = obs_source_get_monitoring_type(source);
	const bool muted = obs_source_muted(source);

	if (monitoring != OBS_MONITORING_TYPE_NONE) {
		obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_NONE);
		if (monitoring == OBS_MONITORING_TYPE_MONITOR_ONLY)
			obs_source_set_muted(source, true);
	} else if (muted) {
		obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_MONITOR_ONLY);
		obs_source_set_muted(source, false);
	} else {
		obs_source_set_monitoring_type(source, OBS_MONITORING_TYPE_MONITOR_AND_OUTPUT);
	}
}

QByteArray WorshipRemote::stateJson() const
{
	const WorshipLive &live = WorshipLive::instance();
	const LiveState state = live.state();
	QJsonObject object{{QStringLiteral("source"), state.source},
			   {QStringLiteral("title"), state.title},
			   {QStringLiteral("section"), state.section},
			   {QStringLiteral("current"), state.current},
			   {QStringLiteral("next"), state.next},
			   {QStringLiteral("index"), state.index},
			   {QStringLiteral("count"), state.count},
			   {QStringLiteral("hidden"), state.hidden},
			   {QStringLiteral("message"), live.stageMessage()},
			   {QStringLiteral("service"), QJsonArray::fromStringList(live.serviceItems())},
			   {QStringLiteral("serviceIndex"), live.serviceCurrent()}};

	const bool studio = obs_frontend_preview_program_mode_active();
	OBSSourceAutoRelease program = programSource();
	OBSSourceAutoRelease preview = studio ? obs_frontend_get_current_preview_scene() : nullptr;
	object.insert(QStringLiteral("studio"), studio);
	object.insert(QStringLiteral("accent"), AppearanceDialog::savedAccent().name());
	object.insert(QStringLiteral("program"), program ? QT_UTF8(obs_source_get_name(program)) : QString());
	object.insert(QStringLiteral("preview"), preview ? QT_UTF8(obs_source_get_name(preview)) : QString());
	object.insert(QStringLiteral("streaming"), obs_frontend_streaming_active());
	/* Projector and stream separately: what the projector shows and what can go to it. */
	const bool separate = WorshipOutput::SeparateRoomScreen();
	object.insert(QStringLiteral("sep"), separate);
	if (separate) {
		object.insert(QStringLiteral("proyector"), WorshipOutput::OnAirSceneName());
		object.insert(QStringLiteral("proyectables"), QJsonArray::fromStringList(WorshipOutput::ProjectorScenes()));
		object.insert(QStringLiteral("contenido"), WorshipOutput::LastContentScene());
	}
	object.insert(QStringLiteral("recording"), obs_frontend_recording_active());

	QJsonArray scenes;
	char **names = obs_frontend_get_scene_names();
	for (char **name = names; name && *name; ++name) {
		const QString scene = QT_UTF8(*name);
		if (userScene(scene))
			scenes.append(scene);
	}
	bfree(names);
	object.insert(QStringLiteral("scenes"), scenes);

	/* Sources of what is on screen, to show/hide them or mute them. */
	QJsonArray sources;
	OBSSourceAutoRelease edited = editedSource();
	if (obs_scene_t *scene = edited ? obs_scene_from_source(edited) : nullptr) {
		obs_scene_enum_items(
			scene,
			[](obs_scene_t *, obs_sceneitem_t *item, void *data) {
				obs_source_t *source = obs_sceneitem_get_source(item);
				const bool audio = obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO;
				const obs_monitoring_type monitoring =
					audio ? obs_source_get_monitoring_type(source) : OBS_MONITORING_TYPE_NONE;
				/* Like the mixer: the speaker shows crossed out also when
				 * the sound only goes to the headphones. */
				const bool silent = audio && (obs_source_muted(source) ||
							      monitoring == OBS_MONITORING_TYPE_MONITOR_ONLY);
				static_cast<QJsonArray *>(data)->prepend(
					QJsonObject{{QStringLiteral("nombre"), QT_UTF8(obs_source_get_name(source))},
						    {QStringLiteral("visible"), obs_sceneitem_visible(item)},
						    {QStringLiteral("audio"), audio},
						    {QStringLiteral("silencio"), silent},
						    {QStringLiteral("auriculares"), monitoring != OBS_MONITORING_TYPE_NONE}});
				return true;
			},
			&sources);
	}
	object.insert(QStringLiteral("sources"), sources);

	/* The YouTube video on screen, to drive it from the phone. */
	const QJsonObject youtube = YouTubeConnect::Status();
	if (!youtube.isEmpty())
		object.insert(QStringLiteral("youtube"), youtube);

	return QJsonDocument(object).toJson(QJsonDocument::Compact);
}
