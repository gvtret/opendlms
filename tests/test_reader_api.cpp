#include "catch.hpp"
#include "opendlms_reader.h"

extern "C" void reader_hal_init(void) {
}

namespace {
struct ReaderIoProbe {
	int writes;
};

int counting_write(void *ctx, const uint8_t *buf, uint32_t len) {
	ReaderIoProbe *probe = static_cast<ReaderIoProbe *>(ctx);
	if ((probe == nullptr) || (buf == nullptr) || (len == 0U)) {
		return -1;
	}
	probe->writes++;
	return static_cast<int>(len);
}

int timeout_read(void *ctx, uint8_t *buf, uint32_t len, uint32_t timeout_ms) {
	(void)ctx;
	(void)buf;
	(void)len;
	(void)timeout_ms;
	return 0;
}
}  // namespace

TEST_CASE("Reader connect rejects incomplete IO before writing", "[reader]") {
	opendlms_reader_t reader;
	csm_asso_config association = {};
	association.llc.ssap = 1U;
	association.llc.dsap = 1U;
	REQUIRE(opendlms_reader_init(&reader, &association, 1U) == 0);

	ReaderIoProbe probe = {};
	opendlms_reader_io_t io = {};
	io.ctx = &probe;
	io.write = counting_write;
	io.read = nullptr;
	io.rx_timeout_ms = OPENDLMS_READER_RX_TIMEOUT_MS;

	opendlms_reader_session_t session;
	opendlms_reader_session_init(&session, &reader, io, &association);

	REQUIRE(opendlms_reader_connect(&session) == -1);
	REQUIRE(session.client == nullptr);
	REQUIRE(probe.writes == 0);
}

TEST_CASE("Reader connect rejects zero wrapper ports before writing", "[reader]") {
	opendlms_reader_t reader;
	csm_asso_config association = {};
	association.llc.ssap = 1U;
	association.llc.dsap = 1U;
	REQUIRE(opendlms_reader_init(&reader, &association, 1U) == 0);

	ReaderIoProbe probe = {};
	opendlms_reader_io_t io = {};
	io.ctx = &probe;
	io.write = counting_write;
	io.read = timeout_read;
	io.rx_timeout_ms = OPENDLMS_READER_RX_TIMEOUT_MS;

	opendlms_reader_session_t session;
	opendlms_reader_session_init(&session, &reader, io, nullptr);

	REQUIRE(opendlms_reader_connect(&session) == -1);
	REQUIRE(session.client == nullptr);
	REQUIRE(probe.writes == 0);
}
