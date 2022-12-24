#include "tmc2209.h"

#include <stdio.h>
#include <stdlib.h>

// hack because of sleep needed
#include "pico/stdlib.h"

namespace tmc2209 {
namespace {

uint8_t computeCrcIncremental(uint8_t crc, uint8_t currentByte) {
  for (int j = 0; j < 8; j++) {
    if ((crc >> 7) ^ (currentByte & 0x01)) {
      crc = (crc << 1) ^ 0x07;
    } else {
      crc = (crc << 1);
    }
    currentByte = currentByte >> 1;
  }

  return crc;
}

uint8_t computeCrc(const uint8_t *datagram, uint8_t datagramLength) {
  uint8_t crc = 0;
  for (int i = 0; i < datagramLength; i++) {
    uint8_t currentByte = datagram[i];
    crc = computeCrcIncremental(crc, currentByte);
  }

  return crc;
}

typedef struct TMC2209Write {
  uint8_t slave;
  uint8_t reg;
  uint32_t data;
} TMC2209Write_t;

typedef struct {
  uint8_t slave;
  uint8_t reg;
} TMC2209ReadReq_t;

typedef struct {
  uint8_t reg;
  uint32_t data;
} TMC2209ReadResp_t;

typedef struct {
  uint8_t buffer[8];
} Buffer8Byte_t;

typedef struct {
  uint8_t buffer[4];
} Buffer4Byte_t;

typedef struct {
  uint8_t v;
} TxCount_t;

Buffer8Byte_t tmc2209_SerializeWrite(const TMC2209Write_t *w) {
  Buffer8Byte_t r;

  r.buffer[0] = 0b00000101;
  r.buffer[1] = w->slave;
  r.buffer[2] = w->reg | 0x80;
  r.buffer[3] = (w->data >> 24) & 0xff;
  r.buffer[4] = (w->data >> 16) & 0xff;
  r.buffer[5] = (w->data >> 8) & 0xff;
  r.buffer[6] = (w->data >> 0) & 0xff;
  r.buffer[7] = computeCrc(r.buffer, 7);

  return r;
}

Buffer4Byte_t tmc2209_SerializeReadReq(const TMC2209ReadReq_t *w) {
  Buffer4Byte_t r;

  r.buffer[0] = 0b00000101;
  r.buffer[1] = w->slave;
  r.buffer[2] = w->reg;
  r.buffer[3] = computeCrc(r.buffer, 3);

  return r;
}

TMC2209ReadResp_t tmc2209_ParseReadResp(const Buffer8Byte_t *b) {
  const uint8_t crc = b->buffer[7];
  const uint32_t data = (b->buffer[6] << 0) | (b->buffer[5] << 8) |
                        (b->buffer[4] << 16) | (b->buffer[3] << 24);
  const uint8_t reg = b->buffer[2];
  const uint8_t master = b->buffer[1];
  const uint8_t preamble = b->buffer[0];

#if 0
    printf("Reg %d is: 0x%x\n", reg, data);

    printf("Dumping response: ");
    for (int i = 0; i < 8; i++) {
        printf("%d, ", b->buffer[i]);
    }
    printf("\n");
#endif

  if ((reg & 0x80) != 0) {
    printf("7th bit not zero");
    abort();
  }

  if (master != 0xff) {
    printf("master not 0xff");
    abort();
  }

  if (preamble != 0b00000101) {
    printf("Incorrect preamble");
    abort();
  }

  if (crc != computeCrc(b->buffer, 7)) {
    printf("Incorrect crc");
    abort();
  }

  TMC2209ReadResp_t resp;
  resp.data = data;
  resp.reg = reg;

  return resp;
}

bool compareTxCount(TxCount_t old, TxCount_t _new) {
  if (old.v != 255) {
    return (_new.v == (old.v + 1));
  } else {
    return (_new.v == 0);
  }
}

typedef struct {
  uint32_t I_scale_analog : 1;
  uint32_t internal_Rsense : 1;
  uint32_t en_SpreadCycle : 1;
  uint32_t shaft : 1;
  uint32_t index_otpw : 1;
  uint32_t index_step : 1;
  uint32_t pdn_disable : 1;
  uint32_t mstep_reg_select : 1;
  uint32_t multistep_filt : 1;
  uint32_t test_mode : 1;
} GCONF_t;

typedef struct {
  uint8_t iHold;
  uint8_t iRun;
  uint8_t iHoldDelay;
} IHoldIRun_t;

typedef struct {
  uint32_t diss2vs : 1;
  uint32_t diss2g : 1;
  uint32_t dedge : 1;
  uint32_t intpol : 1;
  uint32_t mres : 4;
  uint32_t vsense : 1;
  uint32_t tbl : 2;
  uint32_t hend : 4;
  uint32_t hstrt : 3;
  uint32_t toff : 4;
} ChopConf_t;

typedef struct {
  uint32_t result : 10;
} SG_RESULT_t;

typedef struct {
  uint32_t threshold : 8;
} SGTHRS_t;

typedef struct {
  uint32_t threshold : 20;
} TCOOLTHRS_t;

typedef struct {
  uint32_t result : 20;
} TSTEP_t;

template <typename T> struct Register {
  static uint8_t opcode();
  static T parse(uint32_t d);
  static uint32_t serialize(const T *d);
  static void print(const T *d);
};

template <> struct Register<ChopConf_t> {
  static uint8_t opcode() { return 0x6C; }

  static ChopConf_t parse(uint32_t d) {
    ChopConf_t r;

    r.diss2vs = (d & 0x80000000) != 0;
    r.diss2g = (d & 0x40000000) != 0;
    r.dedge = (d & 0x20000000) != 0;
    r.intpol = (d & 0x10000000) != 0;
    r.mres = (d >> 24) & 0x0f;
    r.vsense = (d & 0x00020000) != 0;
    r.tbl = (d >> 15) & 0x03;
    r.hend = (d >> 7) & 0x0f;
    r.hstrt = (d >> 4) & 0x07;
    r.toff = (d >> 0) & 0x0f;

    return r;
  }

  static uint32_t serialize(const ChopConf_t *d) {
    uint32_t r = (d->diss2vs << 31) | (d->diss2g << 30) | (d->dedge << 29) |
                 (d->intpol << 28) | (d->mres << 24) | (d->vsense << 17) |
                 (d->tbl << 15) | (d->hend << 7) | (d->hstrt << 4) |
                 (d->toff << 0);

    return r;
  }

  static void print(const ChopConf_t *d) {
    printf("*** ChopConf ***\n");
    printf("diss2vs %d\n", d->diss2vs);
    printf("diss2g %d\n", d->diss2g);
    printf("dedge %d\n", d->dedge);
    printf("intpol %d\n", d->intpol);
    printf("mres %d\n", d->mres);
    printf("vsense %d\n", d->vsense);
    printf("tbl %d\n", d->tbl);
    printf("hend %d\n", d->hend);
    printf("hstrt %d\n", d->hstrt);
    printf("toff %d\n", d->toff);
    printf("\n");
  }
};

template <> struct Register<SG_RESULT_t> {
  static uint8_t opcode() { return 0x41; }

  static SG_RESULT_t parse(uint32_t d) {
    SG_RESULT_t r;
    r.result = d;
    return r;
  }

  static uint32_t serialize(const SG_RESULT_t *d) { return d->result; }

  static void print(const SG_RESULT_t *d) {
    printf("*** SG_RESULT ***\n");
    printf("result %d\n", d->result);
    printf("\n");
  }
};

template <> struct Register<SGTHRS_t> {
  static uint8_t opcode() { return 0x40; }

  static SGTHRS_t parse(uint32_t d) {
    SGTHRS_t r;
    r.threshold = d;
    return r;
  }

  static uint32_t serialize(const SGTHRS_t *d) { return d->threshold; }

  static void print(const SGTHRS_t *d) {
    printf("*** SGTHRS_t ***\n");
    printf("threshold %d\n", d->threshold);
    printf("\n");
  }
};

template <> struct Register<TCOOLTHRS_t> {
  static uint8_t opcode() { return 0x14; }

  static TCOOLTHRS_t parse(uint32_t d) {
    TCOOLTHRS_t r;
    r.threshold = d;
    return r;
  }

  static uint32_t serialize(const TCOOLTHRS_t *d) { return d->threshold; }

  static void print(const TCOOLTHRS_t *d) {
    printf("*** TCOOLTHRS_t ***\n");
    printf("threshold %d\n", d->threshold);
    printf("\n");
  }
};

template <> struct Register<TSTEP_t> {
  static uint8_t opcode() { return 0x12; }

  static TSTEP_t parse(uint32_t d) {
    TSTEP_t r;
    r.result = d;
    return r;
  }

  static uint32_t serialize(const TSTEP_t *d) { return d->result; }

  static void print(const TSTEP_t *d) {
    printf("*** TSTEP_t ***\n");
    printf("result %d\n", d->result);
    printf("\n");
  }
};

template <> struct Register<IHoldIRun_t> {
  static uint8_t opcode() { return 0x10; }

  static IHoldIRun_t parse(uint32_t d) {
    IHoldIRun_t r;

    r.iHold = (d >> 0) & 0x1f;
    r.iRun = (d >> 8) & 0x1f;
    r.iHoldDelay = (d >> 16) & 0x0f;

    return r;
  }

  static uint32_t serialize(const IHoldIRun_t *d) {
    uint32_t r = (d->iHold << 0) | (d->iRun << 8) | (d->iHoldDelay << 16);

    return r;
  }

  static void print(const IHoldIRun_t *d) {
    printf("*** IHoldIRun ***\n");
    printf("IHold %d\n", d->iHold);
    printf("IRun %d\n", d->iRun);
    printf("IHoldDelay %d\n", d->iHoldDelay);
    printf("\n");
  }
};

template <> struct Register<GCONF_t> {
  static uint8_t opcode() { return 0x00; }

  static GCONF_t parse(uint32_t d) {
    GCONF_t r;

    r.I_scale_analog = (d & 0x01) != 0;
    r.internal_Rsense = (d & 0x02) != 0;
    r.en_SpreadCycle = (d & 0x04) != 0;
    r.shaft = (d & 0x08) != 0;
    r.index_otpw = (d & 0x10) != 0;
    r.index_step = (d & 0x20) != 0;
    r.pdn_disable = (d & 0x40) != 0;
    r.mstep_reg_select = (d & 0x80) != 0;
    r.multistep_filt = (d & 0x100) != 0;
    r.test_mode = (d & 0x200) != 0;

    return r;
  }

  static uint32_t serialize(const GCONF_t *d) {
    uint32_t r = (d->I_scale_analog << 0) | (d->internal_Rsense << 1) |
                 (d->en_SpreadCycle << 2) | (d->shaft << 3) |
                 (d->index_otpw << 4) | (d->index_step << 5) |
                 (d->pdn_disable << 6) | (d->mstep_reg_select << 7) |
                 (d->multistep_filt << 8) | (d->test_mode << 9);

    return r;
  }

  static void print(const GCONF_t *d) {
    printf("*** GCONF ***\n");
    printf("I_scale_analog: %d\n", d->I_scale_analog);
    printf("internal_Rsense: %d\n", d->internal_Rsense);
    printf("en_SpreadCycle: %d\n", d->en_SpreadCycle);
    printf("shaft: %d\n", d->shaft);
    printf("index_otpw: %d\n", d->index_otpw);
    printf("index_step: %d\n", d->index_step);
    printf("pdn_disable: %d\n", d->pdn_disable);
    printf("mstep_reg_select: %d\n", d->mstep_reg_select);
    printf("multistep_filt: %d\n", d->multistep_filt);
    printf("test_mode: %d\n", d->test_mode);
    printf("\n");
  }
};

class DriverImpl : public Driver {
  UARTPtr uart;

  TMC2209ReadResp_t query(const TMC2209ReadReq_t *req) {
    const Buffer4Byte_t b1 = tmc2209_SerializeReadReq(req);

    uart->write(b1.buffer, 4);

    Buffer4Byte_t b2;
    uart->read(b2.buffer, 4);

    Buffer8Byte_t b3;
    uart->read(b3.buffer, 8);

    if (uart->isReadable()) {
      printf("Unexpected data in rx queue\n");
      abort();
    }

    const TMC2209ReadResp_t resp = tmc2209_ParseReadResp(&b3);

    sleep_ms(10); // Race condition. Without it communication hangs

    return resp;
  }

  TxCount_t getTxCount() {
    const TMC2209ReadReq_t req = {0, 0x02};
    TMC2209ReadResp_t q = query(&req);
    TxCount_t r = {(uint8_t)q.data};
    return r;
  }

  void modify(const TMC2209Write_t *w) {
    TxCount_t c1 = getTxCount();

    const Buffer8Byte_t b1 = tmc2209_SerializeWrite(w);
    uart->write(b1.buffer, 8);

    Buffer8Byte_t b2;
    uart->read(b2.buffer, 8);

    TxCount_t c2 = getTxCount();

    if (compareTxCount(c1, c2) == false) {
      printf("Command not accepted\n");
      abort();
    }
  }

  template <typename T> void dump() {
    using Reg = Register<T>;
    const TMC2209ReadReq_t req = {0, Reg::opcode()};
    TMC2209ReadResp_t resp = query(&req);
    T r = Reg::parse(resp.data);
    Reg::print(&r);
  }

  template <typename T> void write2(const T *conf) {
    using Reg = Register<T>;

    const TMC2209ReadReq_t req = {0, Reg::opcode()};

    TMC2209Write_t w = {0, Reg::opcode(), Reg::serialize(conf)};

    modify(&w);

    TMC2209ReadResp_t resp2 = query(&req);
    T conf2 = Reg::parse(resp2.data);
    Reg::print(&conf2);
  }

  template <typename T> void modify2(auto f) {
    using Reg = Register<T>;

    const TMC2209ReadReq_t req = {0, Reg::opcode()};

    TMC2209ReadResp_t resp1 = query(&req);
    T conf1 = Reg::parse(resp1.data);
    Reg::print(&conf1);

    conf1 = f(conf1);

    write2(&conf1);
  }

  void set_gconf() {
    modify2<GCONF_t>([](GCONF_t conf) {
      conf.I_scale_analog = 1;   // use VREF
      conf.pdn_disable = 1;      // use uart
      conf.mstep_reg_select = 0; // use MS1, MS2 pins
      conf.multistep_filt = 0;   // no filtering

      return conf;
    });
  }

  void tmc2209_init_chopconf() {
    modify2<ChopConf_t>([](ChopConf_t conf) {
      conf.vsense = 1;

      return conf;
    });
  }

  void set_IRunIHold() {
    IHoldIRun_t conf;
    conf.iHold = 16;
    conf.iRun = 31;
    conf.iHoldDelay = 15;

    write2(&conf);
  }

  void set_SGTHRS() {
    SGTHRS_t conf;
    conf.threshold = 10;

    write2(&conf);
  }

  void set_TCOOLTHRS_t() {
    TCOOLTHRS_t r;
    r.threshold = 130;
    write2(&r);
  }

public:
  DriverImpl(UARTPtr uart) : uart(std::move(uart)) {}
  virtual void initialize() {
    printf("Initialization\n");

    set_gconf();
    set_IRunIHold();
    set_SGTHRS();
    set_TCOOLTHRS_t();
  }
};

} // namespace

std::unique_ptr<Driver> create(UARTPtr ptr) {
  return std::make_unique<DriverImpl>(std::move(ptr));
}

} // namespace tmc2209