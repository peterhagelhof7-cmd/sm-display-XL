#include "SnmpClient.h"

#include <string.h>
#include <vector>

namespace {

// Minimaler BER-TLV-Puffer: reicht fuer diese kleinen SNMP-v1-Pakete
// (alle Laengen < 128 Byte, daher nur Short-Form-Laengen noetig).
struct BerBuffer {
	uint8_t data[192];
	size_t len = 0;

	void byte(uint8_t b) {
		if (len < sizeof(data)) data[len++] = b;
	}
	void bytes(const uint8_t *p, size_t n) {
		for (size_t i = 0; i < n; i++) byte(p[i]);
	}
	void tlv(uint8_t tag, const uint8_t *content, size_t contentLen) {
		byte(tag);
		byte(static_cast<uint8_t>(contentLen));
		bytes(content, contentLen);
	}
};

void encodeOid(const String &dotted, BerBuffer &out) {
	std::vector<uint32_t> parts;
	int start = 0;
	while (start <= static_cast<int>(dotted.length())) {
		int dot = dotted.indexOf('.', start);
		String tok = (dot < 0) ? dotted.substring(start) : dotted.substring(start, dot);
		if (tok.length() > 0) parts.push_back(static_cast<uint32_t>(tok.toInt()));
		if (dot < 0) break;
		start = dot + 1;
	}

	BerBuffer content;
	if (parts.size() >= 2) {
		content.byte(static_cast<uint8_t>(parts[0] * 40 + parts[1]));
	}
	for (size_t i = 2; i < parts.size(); i++) {
		uint32_t v = parts[i];
		uint8_t tmp[5];
		int n = 0;
		tmp[n++] = v & 0x7F;
		v >>= 7;
		while (v > 0) {
			tmp[n++] = (v & 0x7F) | 0x80;
			v >>= 7;
		}
		for (int j = n - 1; j >= 0; j--) content.byte(tmp[j]);
	}
	out.tlv(0x06, content.data, content.len);
}

void encodeInteger(int32_t value, BerBuffer &out) {
	uint8_t tmp[4];
	int n = 0;
	uint32_t v = static_cast<uint32_t>(value);
	bool negative = value < 0;
	do {
		tmp[n++] = v & 0xFF;
		v >>= 8;
	} while ((negative ? (static_cast<int32_t>(v) != -1) : (v != 0)) && n < 4);

	bool msbSet = (tmp[n - 1] & 0x80) != 0;
	if (msbSet != negative && n < 4) {
		tmp[n++] = negative ? 0xFF : 0x00;
	}

	uint8_t rev[5];
	for (int i = 0; i < n; i++) rev[i] = tmp[n - 1 - i];
	out.tlv(0x02, rev, n);
}

void encodeOctetString(const String &s, BerBuffer &out) {
	out.tlv(0x04, reinterpret_cast<const uint8_t *>(s.c_str()), s.length());
}

void encodeNull(BerBuffer &out) {
	out.tlv(0x05, nullptr, 0);
}

} // namespace

bool SnmpClient::getRaw(const String &host, const String &community, const String &oidDotted, uint8_t wantTag,
                         uint8_t *outContent, size_t &outLen, uint16_t port, uint32_t timeoutMs) {
	// Varbind: SEQUENCE { OID, NULL }
	BerBuffer oidBuf;
	encodeOid(oidDotted, oidBuf);
	BerBuffer nullBuf;
	encodeNull(nullBuf);
	BerBuffer varbind;
	varbind.bytes(oidBuf.data, oidBuf.len);
	varbind.bytes(nullBuf.data, nullBuf.len);
	BerBuffer varbindSeq;
	varbindSeq.tlv(0x30, varbind.data, varbind.len);
	BerBuffer varbindList;
	varbindList.tlv(0x30, varbindSeq.data, varbindSeq.len);

	// PDU: GetRequest (0xA0) { requestId, errorStatus(0), errorIndex(0), varbindList }
	BerBuffer reqId;
	encodeInteger(static_cast<int32_t>(millis()), reqId);
	BerBuffer errStatus;
	encodeInteger(0, errStatus);
	BerBuffer errIndex;
	encodeInteger(0, errIndex);

	BerBuffer pduContent;
	pduContent.bytes(reqId.data, reqId.len);
	pduContent.bytes(errStatus.data, errStatus.len);
	pduContent.bytes(errIndex.data, errIndex.len);
	pduContent.bytes(varbindList.data, varbindList.len);

	BerBuffer pdu;
	pdu.tlv(0xA0, pduContent.data, pduContent.len);

	// Message: SEQUENCE { version(1=v2c), community, PDU }
	// Bis 2026-07-21 v1 (0) - Umstellung auf v2c: Gegenstelle (SNMP_Agent-
	// Bibliothek, siehe sensormeter/-poe/-wlan SNMPManager.cpp) verhandelt
	// die Version ohnehin pro Anfrage automatisch (SNMPPacket.cpp liest sie
	// direkt aus dem eingehenden Paket, keine feste Server-Version) - auf
	// Client-Seite reicht daher diese einzelne Konstante. PDU-Form/
	// Response-Parsing bleiben unveraendert kompatibel (v2c nutzt fuer GET/
	// GetResponse dieselben Tags wie v1, neue v2c-Faehigkeiten wie GetBulk/
	// Counter64 werden hier nicht genutzt). Sicherheitsmodell bleibt
	// unveraendert (Community-String weiterhin im Klartext) - siehe
	// docs/entscheidungen.md fuer die Abwaegung ggue. SNMPv3.
	BerBuffer version;
	encodeInteger(1, version);
	BerBuffer communityBuf;
	encodeOctetString(community, communityBuf);

	BerBuffer msgContent;
	msgContent.bytes(version.data, version.len);
	msgContent.bytes(communityBuf.data, communityBuf.len);
	msgContent.bytes(pdu.data, pdu.len);

	BerBuffer message;
	message.tlv(0x30, msgContent.data, msgContent.len);

	IPAddress ip;
	if (!ip.fromString(host)) {
		return false;
	}

	udp.begin(0);
	udp.beginPacket(ip, port);
	udp.write(message.data, message.len);
	udp.endPacket();

	uint32_t start = millis();
	int packetSize = 0;
	while ((millis() - start) < timeoutMs) {
		packetSize = udp.parsePacket();
		if (packetSize > 0) break;
		delay(10);
	}
	if (packetSize <= 0) {
		udp.stop();
		return false;
	}

	uint8_t resp[256];
	int n = udp.read(resp, sizeof(resp));
	udp.stop();
	if (n <= 0) {
		return false;
	}

	// Antwort einfach linear als Folge von TLVs abgehen: bei zusammengesetzten
	// Typen (SEQUENCE/PDU, Tags 0x30/0xA0/0xA2) einen Schritt "hinein" gehen
	// statt darueber hinweg zu springen, bei allen anderen darueber hinweg.
	// Da unsere Antwort ausschliesslich verschachtelt ist (keine Geschwister
	// auf oberster Ebene), liefert das den letzten Element mit passendem Tag
	// im Baum - bei einem Einzel-Varbind-GetResponse genau der gesuchte Wert.
	// Deckt keine SNMP-Fehlerantworten (errorStatus != 0) gesondert ab, siehe
	// docs/entscheidungen.md.
	int idx = 0;
	bool found = false;
	int foundStart = 0, foundLen = 0;
	while (idx < n - 1) {
		uint8_t tag = resp[idx];
		uint8_t lenByte = resp[idx + 1];
		if (lenByte & 0x80) {
			break; // Long-form-Laenge nicht unterstuetzt, Pakete sind hier klein
		}
		int contentLen = lenByte;
		int contentStart = idx + 2;
		if (contentStart + contentLen > n) break;

		if (tag == wantTag) {
			foundStart = contentStart;
			foundLen = contentLen;
			found = true;
		}

		if (tag == 0x30 || tag == 0xA0 || tag == 0xA2) {
			idx = contentStart;
		} else {
			idx = contentStart + contentLen;
		}
	}

	if (!found || static_cast<size_t>(foundLen) > outLen) {
		return false;
	}
	memcpy(outContent, resp + foundStart, foundLen);
	outLen = static_cast<size_t>(foundLen);
	return true;
}

bool SnmpClient::getInteger(const String &host, const String &community, const String &oidDotted,
                             int32_t &outValue, uint16_t port, uint32_t timeoutMs) {
	uint8_t content[8];
	size_t len = sizeof(content);
	if (!getRaw(host, community, oidDotted, 0x02, content, len, port, timeoutMs)) {
		return false;
	}
	int32_t v = 0;
	if (len > 0 && (content[0] & 0x80)) v = -1;
	for (size_t i = 0; i < len; i++) {
		v = (v << 8) | content[i];
	}
	outValue = v;
	return true;
}

bool SnmpClient::getTimeTicks(const String &host, const String &community, const String &oidDotted,
                               uint32_t &outValue, uint16_t port, uint32_t timeoutMs) {
	uint8_t content[8];
	size_t len = sizeof(content);
	if (!getRaw(host, community, oidDotted, 0x43, content, len, port, timeoutMs)) {
		return false;
	}
	uint32_t v = 0;
	for (size_t i = 0; i < len; i++) {
		v = (v << 8) | content[i];
	}
	outValue = v;
	return true;
}

bool SnmpClient::getString(const String &host, const String &community, const String &oidDotted,
                            String &outValue, uint16_t port, uint32_t timeoutMs) {
	uint8_t content[128];
	size_t len = sizeof(content);
	if (!getRaw(host, community, oidDotted, 0x04, content, len, port, timeoutMs)) {
		return false;
	}
	char buf[129];
	memcpy(buf, content, len);
	buf[len] = '\0';
	outValue = String(buf);
	return true;
}
