/*
 * Button behaviour simulator.
 *
 *   sim_<variant>.exe SEED   -> runs one randomized button session and prints a
 *                              trace of every observable effect, one per line:
 *                              "<ms> <what happened>"
 *
 * The same seed gives the same button session for every variant, so two
 * implementations behave the same exactly when their traces are identical.
 */
#include "sim.h"

/* ======================================================================== */
/* trace                                                                     */
/* ======================================================================== */
static uint32_t m_now = 0;

static void trace(const char *fmt, ...)
{
  va_list ap;
  printf("%7u ", m_now);
  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);
  printf("\n");
}

static void trace_print(const char *kind, const char *fmt, va_list ap)
{
  char buf[256];
  vsnprintf(buf, sizeof(buf), fmt, ap);
  trace("%s \"%s\"", kind, buf);
}

void sl_zigbee_app_debug_println(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); trace_print("app_println", fmt, ap); va_end(ap); }
void sl_zigbee_core_debug_print(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); trace_print("core_print", fmt, ap); va_end(ap); }
void sl_zigbee_core_debug_println(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); trace_print("core_println", fmt, ap); va_end(ap); }

/* ======================================================================== */
/* GPIO                                                                      */
/* ======================================================================== */
sim_gpio_t sim_gpio;

void sim_gpio_sync(void)
{
  sim_gpio.IF &= ~sim_gpio.IF_CLR;
  sim_gpio.IF_CLR = 0;
  sim_gpio.IEN |= sim_gpio.IEN_SET;
  sim_gpio.IEN_SET = 0;
}

void GPIO_PinModeSet(int port, unsigned pin, int mode, unsigned out)
{ (void)mode; if(out) { sim_gpio.P[port].DIN |= (1UL << pin); } }
int32_t sl_hal_gpio_configure_external_interrupt(const sl_gpio_t *gpio, int32_t int_no, int flags)
{ (void)gpio; (void)flags; return int_no; }
void sl_clock_manager_enable_bus_clock(int clock) { (void)clock; }
void NVIC_EnableIRQ(int irq) { (void)irq; }

// Button pins: T = PB1 (int 1), D = PD3 (int 3); low = pressed
static const struct { uint8_t port, pin, int_no; } m_pins[2] = { { 1, 1, 1 }, { 3, 3, 3 } };

static void sim_set_pin(uint8_t button, bool pressed)
{
  uint32_t bit = 1UL << m_pins[button].pin;
  bool was_low = (0 == (sim_gpio.P[m_pins[button].port].DIN & bit));
  if(was_low == pressed)
  {
     return;   // no edge
  }
  if(pressed) { sim_gpio.P[m_pins[button].port].DIN &= ~bit; }
  else        { sim_gpio.P[m_pins[button].port].DIN |= bit; }
  sim_gpio_sync();
  sim_gpio.IF |= (1UL << m_pins[button].int_no);
  if(sim_gpio.IEN & sim_gpio.IF)
  {
     GPIO_ODD_IRQHandler();
  }
}

/* ======================================================================== */
/* time, one-shot timers (3 slots, like hal_utils.c), events, tasker         */
/* ======================================================================== */
uint32_t get_time_ms(void) { return m_now; }
bool has_timed_out(uint32_t timeStamp, uint32_t delay) { return((get_time_ms() - timeStamp) >= delay); }

static struct { void (*func)(void); uint32_t due; } m_one_shot[3];

int8_t set_one_shot_us_timer_isr(void (*func)(void), uint32_t delay_us)
{
  if((60UL > delay_us) || (1700000UL < delay_us) || (NULL == func))
  {
     return(false);   // (sic) same as hal_utils.c
  }
  for(int8_t i = 0; i < 3; i++)
  {
     if(NULL == m_one_shot[i].func)
     {
        m_one_shot[i].func = func;
        m_one_shot[i].due = m_now + (delay_us / 1000UL);
        return(i);
     }
  }
  return(-1);
}

bool disable_one_shot_us_timer_isr(int8_t int_num)
{
  if((0 <= int_num) && (int_num < 3))
  {
     m_one_shot[int_num].func = NULL;
     return(true);
  }
  return(false);
}

#define SIM_MAX_EVENTS 16
static sl_zigbee_af_event_t *m_events[SIM_MAX_EVENTS];
static int m_n_events = 0;

static void sim_event_register(sl_zigbee_af_event_t *e, void (*handler)(sl_zigbee_af_event_t *))
{
  e->handler = handler;
  e->scheduled = false;
  e->due = 0;
  for(int i = 0; i < m_n_events; i++) { if(m_events[i] == e) { return; } }
  assert(m_n_events < SIM_MAX_EVENTS);
  m_events[m_n_events++] = e;
}
void sl_zigbee_af_event_init(sl_zigbee_af_event_t *e, void (*h)(sl_zigbee_af_event_t *)) { sim_event_register(e, h); }
void sl_zigbee_af_isr_event_init(sl_zigbee_af_event_t *e, void (*h)(sl_zigbee_af_event_t *)) { sim_event_register(e, h); }
void sl_zigbee_af_event_set_active(sl_zigbee_af_event_t *e) { e->scheduled = true; e->due = m_now; }
void sl_zigbee_af_event_set_delay_ms(sl_zigbee_af_event_t *e, uint32_t ms) { e->scheduled = true; e->due = m_now + ms; }
void sl_zigbee_af_event_set_inactive(sl_zigbee_af_event_t *e) { e->scheduled = false; }
bool sl_zigbee_af_event_is_scheduled(sl_zigbee_af_event_t *e) { return e->scheduled; }

#define N_TASKS 8
static void (*m_tasks[N_TASKS])(taskParm_t, taskParm_t);
static taskParm_t m_task_p0[N_TASKS], m_task_p1[N_TASKS];

uint8_t removeTask(void (*funcPtr)())
{
  for(uint8_t i = 0; i < N_TASKS; i++)
  {
     if(m_tasks[i] == (void (*)(taskParm_t, taskParm_t))funcPtr)
     {
        m_tasks[i] = NULL;
        m_task_p0[i].u32 = 0;
        m_task_p1[i].u32 = 0;
        return i;
     }
  }
  return 0xFF;
}

uint8_t addTask(void (*funcPtr)(), taskParm_t parm0, taskParm_t parm1)
{
  removeTask(funcPtr);
  for(uint8_t i = 0; i < N_TASKS; i++)
  {
     if(NULL == m_tasks[i])
     {
        m_tasks[i] = (void (*)(taskParm_t, taskParm_t))funcPtr;
        m_task_p0[i] = parm0;
        m_task_p1[i] = parm1;
        return i;
     }
  }
  assert(0);
  return 0xFF;
}

/* ======================================================================== */
/* application stubs: model just enough state, trace every call              */
/* ======================================================================== */
static struct
{
  which_nwk_t nwk;
  bool in_group;
  uint16_t color;
  uint8_t level, max, mid, min, pwm;
  uint32_t flags;
  uint8_t tokens[SIM_N_TOKENS];
  uint16_t unocc, unocc_off, threshold;
} s;

void force_factory_reset(uint32_t delay_ms, reset_reason_t r) { trace("force_factory_reset(%u, %d)", delay_ms, r); }
bool hal_start_flashing_red_led(uint32_t n, uint32_t on, uint32_t off) { trace("flash_red(%u, %u, %u)", n, on, off); return true; }
bool hal_start_flashing_green_led(uint32_t n, uint32_t on, uint32_t off) { trace("flash_green(%u, %u, %u)", n, on, off); return true; }
void hal_set_red_led(uint8_t on) { trace("red_led(%u)", on); }
void hal_set_green_led(uint8_t on) { trace("green_led(%u)", on); }
void save_min_light_level(uint8_t v) { s.min = v; trace("save_min_light_level(%u)", v); }
void save_mid_light_level(uint8_t v) { s.mid = v; trace("save_mid_light_level(%u)", v); }
void save_max_light_level(uint8_t v) { s.max = v; trace("save_max_light_level(%u)", v); }
uint8_t get_min_light_level(void) { return s.min; }
uint8_t get_mid_light_level(void) { return s.mid; }
uint8_t get_max_light_level(void) { return s.max; }
uint16_t read_light_level_attribute(uint16_t id) { (void)id; return s.level; }
bool is_app_flags_nonvol_set(uint32_t mask) { return (s.flags & mask) != 0; }
void set_app_flags_nonvol(uint32_t mask, uint8_t v) { if(v) { s.flags |= mask; } else { s.flags &= ~mask; } trace("set_app_flags(0x%x, %u)", mask, v); }
void sl_token_get_data(uint32_t t, uint32_t idx, void *d, uint32_t len) { (void)idx; assert(len == 1); *(uint8_t *)d = s.tokens[t]; }
void sl_token_set_data(uint32_t t, uint32_t idx, void *d, uint32_t len) { (void)idx; assert(len == 1); s.tokens[t] = *(uint8_t *)d; trace("token_set(%u, %u)", t, s.tokens[t]); }
void move_to_level_onoff(uint8_t level, uint16_t tt) { s.level = level; trace("move_to_level_onoff(%u, %u)", level, tt); }
uint8_t get_present_light_level(void) { return s.level; }
uint16_t get_light_color(void) { return s.color; }
void save_light_color(uint16_t k) { s.color = k; trace("save_light_color(%u)", k); }
bool tx_color_new_temp(uint16_t k) { trace("tx_color_new_temp(%u)", k); return true; }
bool tx_new_light_levels(void) { trace("tx_new_light_levels()"); return true; }
bool tx_occ_sensor_new_delays(uint16_t d, uint16_t o) { trace("tx_occ_sensor_new_delays(%u, %u)", d, o); return true; }
void save_adaptiv_unoccupied_delay(uint16_t d) { s.unocc = d; trace("save_unoccupied_delay(%u)", d); }
void save_adaptiv_unoccupied_delay_to_off(uint16_t d) { s.unocc_off = d; trace("save_unoccupied_delay_to_off(%u)", d); }
void set_occupancy_threshold(uint16_t t) { s.threshold = t; trace("set_occupancy_threshold(%u)", t); }
uint16_t get_unoccupied_delay(void) { return s.unocc; }
uint16_t get_unoccupied_delay_to_off(void) { return s.unocc_off; }
void place_in_manual_add_into_group_mode(void) { s.flags |= B_MANUAL_ADD_MODE; trace("place_in_manual_add_into_group_mode()"); }
void broadcast_autocommission_announce(void) { trace("broadcast_autocommission_announce()"); }
which_nwk_t get_present_network(void) { return s.nwk; }
bool device_is_in_a_group(void) { return s.in_group; }
uint8_t hal_get_dim_pwm_255(void) { return s.pwm; }
void hal_set_dim_pwm_255(uint8_t v) { s.pwm = v; trace("set_dim_pwm(%u)", v); }
void hal_start_occ_sensor(void) { trace("start_occ_sensor()"); }
void hal_stop_occ_sensor(void) { trace("stop_occ_sensor()"); }

/* ======================================================================== */
/* randomized button session                                                 */
/* ======================================================================== */
static uint32_t m_rng;
static uint32_t rnd(void) { m_rng ^= m_rng << 13; m_rng ^= m_rng >> 17; m_rng ^= m_rng << 5; return m_rng; }
static uint32_t rnd_range(uint32_t lo, uint32_t hi) { return lo + (rnd() % (hi - lo + 1)); }
static bool chance(uint32_t pct) { return (rnd() % 100) < pct; }

typedef struct { uint32_t t; uint8_t kind; uint8_t a; uint8_t b; } input_t;
enum { IN_PIN, IN_NET };
#define MAX_INPUTS 20000
static input_t m_in[MAX_INPUTS];
static int m_n_in = 0;

static void add_in(uint32_t t, uint8_t kind, uint8_t a, uint8_t b)
{
  assert(m_n_in < MAX_INPUTS);
  m_in[m_n_in++] = (input_t){ t, kind, a, b };
}

static int cmp_in(const void *x, const void *y)
{
  const input_t *p = x, *q = y;
  if(p->t != q->t) { return (p->t < q->t) ? -1 : 1; }
  return (p < q) ? -1 : 1;  // stable
}

// presses of one button; returns end time
static uint32_t gen_presses(uint8_t button, uint32_t t, int n, bool clean)
{
  for(int i = 0; i < n; i++)
  {
     uint32_t r = rnd() % 100;
     uint32_t down;
     if(!clean && (r < 10))      { down = rnd_range(5, 74); }    // fails debounce
     else if(!clean && (r < 20)) { down = rnd_range(72, 78); }   // at the 75 ms debounce limit
     else if(!clean && (r < 25)) { down = rnd_range(480, 520); } // last press at the 2 s hold limit
     else                        { down = rnd_range(80, 350); }
     add_in(t, IN_PIN, button, 1);
     add_in(t + down, IN_PIN, button, 0);
     r = rnd() % 100;
     uint32_t gap;
     if(clean || (r < 85))  { gap = rnd_range(80, 600); }
     else if(r < 92)        { gap = rnd_range(1495, 1505); }     // at the 1.5 s end of sequence
     else                   { gap = rnd_range(600, 1700); }
     t += down + gap;
  }
  return t;
}

static uint32_t gen_hold(uint8_t button, uint32_t t)
{
  uint32_t dur = chance(80) ? rnd_range(3000, 15000) : rnd_range(300, 3999);
  add_in(t, IN_PIN, button, 1);
  add_in(t + dur, IN_PIN, button, 0);
  return t + dur;
}

static void generate(uint32_t seed, int gestures)
{
  static const uint8_t targets[][2] = { {2,2}, {4,4}, {6,6}, {8,8}, {10,10}, {9,11}, {11,9}, {10,9} };
  uint32_t t = 500;

  m_rng = seed * 2654435761u + 1;
  add_in(0, IN_NET, rnd_range(0, 4), chance(50));

  for(int g = 0; g < gestures; g++)
  {
     t += chance(70) ? rnd_range(1600, 4000) : rnd_range(0, 1600);
     if(chance(30))
     {
        add_in(t, IN_NET, rnd_range(0, 4), chance(50));
     }

     uint32_t kind = rnd() % 100;
     uint32_t end;
     if(kind < 45)
     {
        int nt, nd;
        bool clean = chance(60);   // clean: every press counts, so table entries are hit
        uint32_t pick = rnd() % 100;
        if(pick < 30)      { int k = rnd() % 8; nt = targets[k][0]; nd = targets[k][1]; }
        else if(pick < 55) { nt = 0; nd = rnd_range(1, 13); }
        else if(pick < 75) { nt = rnd_range(1, 7); nd = 0; }
        else               { nt = rnd_range(0, 13); nd = rnd_range(0, 13); }
        uint32_t e0 = gen_presses(0, t, nt, clean);
        uint32_t e1 = gen_presses(1, t + rnd_range(0, 300), nd, clean);
        end = (e0 > e1) ? e0 : e1;
     }
     else if(kind < 60) { end = gen_hold(0, t); }
     else if(kind < 75) { end = gen_hold(1, t); }
     else if(kind < 90)
     {
        // both held, staggered press and release
        uint32_t dur = chance(80) ? rnd_range(3000, 15000) : rnd_range(300, 3999);
        uint32_t s0 = rnd_range(0, 1200), s1 = rnd_range(0, 1200);
        add_in(t, IN_PIN, 0, 1);
        add_in(t + s0, IN_PIN, 1, 1);
        add_in(t + s0 + dur, IN_PIN, 0, 0);
        add_in(t + s0 + dur + s1, IN_PIN, 1, 0);
        end = t + s0 + dur + s1;
     }
     else
     {
        // contact bounce
        uint8_t b = rnd() % 2;
        int k = rnd_range(1, 6);
        uint32_t tt = t;
        for(int i = 0; i < k; i++)
        {
           add_in(tt, IN_PIN, b, 1);
           tt += rnd_range(1, 8);
           add_in(tt, IN_PIN, b, 0);
           tt += rnd_range(1, 8);
        }
        end = tt;
     }
     t = end;
  }
  add_in(t + 20000, IN_NET, 0xFF, 0);   // end marker
  qsort(m_in, m_n_in, sizeof(m_in[0]), cmp_in);
}

/* ======================================================================== */
/* main loop                                                                 */
/* ======================================================================== */
int main(int argc, char **argv)
{
  uint32_t seed = (argc > 1) ? (uint32_t)strtoul(argv[1], NULL, 0) : 1;
  int gestures = (argc > 2) ? atoi(argv[2]) : 40;

  memset(&s, 0, sizeof(s));
  s.color = 4000; s.level = 128; s.max = 255; s.mid = 51; s.min = 0; s.pwm = 128;
  s.unocc = 1800; s.unocc_off = 300; s.threshold = 80;
  for(int p = 0; p < 4; p++) { sim_gpio.P[p].DIN = 0xFFFF; }   // pull-ups: released

  generate(seed, gestures);
  sim_variant_init();

  int next = 0;
  for(m_now = 0; ; m_now++)
  {
     // 1. inputs (pin edges run the GPIO interrupt immediately)
     for(; (next < m_n_in) && (m_in[next].t == m_now); next++)
     {
        if(IN_PIN == m_in[next].kind)
        {
           sim_set_pin(m_in[next].a, m_in[next].b);
        }
        else if(0xFF == m_in[next].a)
        {
           return 0;   // end of session
        }
        else
        {
           s.nwk = (which_nwk_t)m_in[next].a;
           s.in_group = m_in[next].b;
           trace("-- network=%d in_group=%d", s.nwk, s.in_group);
        }
     }
     // 2. one-shot timer interrupts
     for(int i = 0; i < 3; i++)
     {
        if((NULL != m_one_shot[i].func) && ((int32_t)(m_now - m_one_shot[i].due) >= 0))
        {
           void (*f)(void) = m_one_shot[i].func;
           m_one_shot[i].func = NULL;
           f();
        }
     }
     // 3. events (main context), in registration order
     for(int i = 0; i < m_n_events; i++)
     {
        sl_zigbee_af_event_t *e = m_events[i];
        if(e->scheduled && ((int32_t)(m_now - e->due) >= 0))
        {
           e->scheduled = false;
           e->handler(e);
        }
     }
     // 4. tasker
     for(int i = 0; i < N_TASKS; i++)
     {
        if(NULL != m_tasks[i])
        {
           m_tasks[i](m_task_p0[i], m_task_p1[i]);
        }
     }
  }
}
