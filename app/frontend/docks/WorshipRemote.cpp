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
#include "WorshipLive.hpp"
#include "WorshipOutput.hpp"

#include <qrcodegen.hpp>
#include <qt-wrappers.hpp>

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
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

/* The phone page: no external files, works offline on the church network. */
const char *const RemotePage = R"HTML(<!doctype html>
<html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0E0F14"><title>WorshipStudio · Control</title>
<style>
:root{--bg:#0E0F14;--s1:#16181F;--s2:#1D2029;--line:#2A2D38;--t:#F2F2F5;--m:#9AA0B4;--a:#636BE6;--g:#E8C46A}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;background:var(--bg);color:var(--t);font:16px/1.45 -apple-system,system-ui,"Segoe UI",Roboto,sans-serif;padding:max(16px,env(safe-area-inset-top)) 16px max(16px,env(safe-area-inset-bottom))}
header{display:flex;align-items:center;justify-content:space-between;margin-bottom:14px}
.brand{font-weight:700;letter-spacing:-.02em}.live{font-size:13px;color:var(--m)}
.live b{display:inline-block;width:8px;height:8px;border-radius:50%;background:#3DD68C;margin-right:6px}
.card{background:var(--s1);border:1px solid var(--line);border-radius:18px;padding:16px;margin-bottom:12px}
.k{font-size:12px;letter-spacing:.1em;text-transform:uppercase;color:var(--m);margin-bottom:6px}
#title{font-weight:700;font-size:18px}#section{color:var(--g);font-size:14px;margin-top:2px}
#current{font-size:22px;font-weight:600;white-space:pre-line;margin-top:10px;min-height:2.6em}
#next{color:var(--m);white-space:pre-line}
.nav{display:grid;grid-template-columns:1fr 1.6fr;gap:10px;margin-bottom:10px}
button{font:600 18px/1 inherit;font-family:inherit;border:0;border-radius:16px;padding:22px 10px;color:var(--t);background:var(--s2);border:1px solid var(--line)}
button:active{transform:scale(.98)}.primary{background:var(--a);border-color:var(--a);color:#fff;font-size:22px}
.row{display:grid;grid-template-columns:1fr;gap:10px}.on{background:var(--g);color:#1A1407;border-color:var(--g)}
ol{list-style:none;margin:0;padding:0}li{padding:12px;border-radius:12px;display:flex;gap:10px;align-items:center}
li+li{border-top:1px solid var(--line)}li.cur{background:var(--s2);color:#fff;font-weight:600}
li span{color:var(--m);font-size:13px;min-width:22px}
#login{display:none}#login input{width:100%;font:600 28px/1 inherit;letter-spacing:.4em;text-align:center;padding:16px;border-radius:14px;border:1px solid var(--line);background:var(--s2);color:var(--t);margin:10px 0}
.err{color:#E85E75;font-size:14px;min-height:1.2em}
</style></head><body>
<header><div class="brand">WorshipStudio</div><div class="live" id="status"><b></b>Conectado</div></header>
<section id="login" class="card"><div class="k">Código de acceso</div>
<p style="margin:0;color:var(--m)">Está en el panel <b>En vivo</b> de WorshipStudio.</p>
<input id="pin" inputmode="numeric" maxlength="4" autocomplete="one-time-code"><div class="err" id="err"></div>
<button class="primary" style="width:100%" onclick="savePin()">Entrar</button></section>
<main id="app">
<div class="card"><div class="k" id="source">En pantalla</div><div id="title">Nada en pantalla</div><div id="section"></div><div id="current"></div></div>
<div class="nav"><button onclick="act('anterior')" aria-label="Anterior">◀︎ Anterior</button><button class="primary" onclick="act('siguiente')" aria-label="Siguiente">Siguiente ▶︎</button></div>
<div class="row"><button id="hide" onclick="act('ocultar')">Ocultar pantalla</button></div>
<div class="card" style="margin-top:12px"><div class="k">Siguiente</div><div id="next">—</div></div>
<div class="card" id="svc" style="display:none"><div class="k">Orden del culto</div><ol id="items"></ol></div>
</main>
<script>
let pin=localStorage.getItem('wsPin')||new URLSearchParams(location.search).get('pin')||'';
if(new URLSearchParams(location.search).get('pin')){localStorage.setItem('wsPin',pin);history.replaceState(null,'','/');}
const $=id=>document.getElementById(id);
function showLogin(msg){$('login').style.display='block';$('app').style.display='none';$('err').textContent=msg||'';}
function savePin(){pin=$('pin').value.trim();localStorage.setItem('wsPin',pin);$('login').style.display='none';$('app').style.display='block';poll();}
async function call(path,opts){const r=await fetch(path+(path.includes('?')?'&':'?')+'pin='+encodeURIComponent(pin),opts);if(r.status===401){showLogin('Código incorrecto');throw 0;}return r.json();}
function render(s){$('source').textContent=s.source||'En pantalla';$('title').textContent=s.title||'Nada en pantalla';
$('section').textContent=(s.section||'')+(s.count?'  ·  '+(s.index+1)+'/'+s.count:'');$('current').textContent=s.current||'';
$('next').textContent=s.next||'—';$('hide').classList.toggle('on',!!s.hidden);$('hide').textContent=s.hidden?'Mostrar pantalla':'Ocultar pantalla';
const it=s.service||[];$('svc').style.display=it.length?'block':'none';
$('items').innerHTML=it.map((t,i)=>'<li class="'+(i===s.serviceIndex?'cur':'')+'" onclick="act(\'item\','+i+')"><span>'+(i+1)+'</span>'+t.replace(/</g,'&lt;')+'</li>').join('');
$('status').innerHTML='<b></b>Conectado';}
async function act(a,n){if(navigator.vibrate)navigator.vibrate(12);try{render(await call('/api/accion?a='+a+(n!==undefined?'&n='+n:''),{method:'POST'}));}catch(e){}}
async function poll(){try{render(await call('/api/estado'));}catch(e){if(e!==0)$('status').innerHTML='<b style="background:#E85E75"></b>Sin conexión';}}
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
			if (ip.isPrivateUse())
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

	int status = 200;
	QByteArray contentType;
	const QByteArray body = respond(requestLine.at(0), QString::fromUtf8(requestLine.at(1)), &status, &contentType);
	const QByteArray reason = status == 200 ? "OK" : status == 401 ? "Unauthorized" : "Not Found";
	QByteArray response = "HTTP/1.1 " + QByteArray::number(status) + ' ' + reason + "\r\n";
	response += "Content-Type: " + contentType + "\r\n";
	response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
	response += "Cache-Control: no-store\r\nConnection: close\r\n\r\n";
	response += body;
	socket->write(response);
	socket->disconnectFromHost();
}

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
		return QByteArray(RemotePage);
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
		if (action == QStringLiteral("siguiente"))
			live.next();
		else if (action == QStringLiteral("anterior"))
			live.previous();
		else if (action == QStringLiteral("ocultar"))
			live.toggleHidden();
		else if (action == QStringLiteral("item"))
			live.requestServiceItem(query.queryItemValue(QStringLiteral("n")).toInt());
	}
	return stateJson();
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
	return QJsonDocument(object).toJson(QJsonDocument::Compact);
}
