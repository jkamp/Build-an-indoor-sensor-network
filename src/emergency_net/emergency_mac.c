#include "emergency_net/emergency_mac.h"

static const struct radio_driver *radio;
static void (* receiver_callback)(const struct mac_driver *);
/*---------------------------------------------------------------------------*/
static int
send_packet(void)
{
  if(radio->send(packetbuf_hdrptr(), packetbuf_totlen()) == RADIO_TX_OK) {
    return MAC_TX_OK;
  }
  return MAC_TX_ERR;
}
/*---------------------------------------------------------------------------*/
static void
input_packet(const struct radio_driver *d)
{
  if(receiver_callback) {
    receiver_callback(&nullmac_driver);
  }
}
/*---------------------------------------------------------------------------*/
static int
read_packet(void)
{
  int len;
  packetbuf_clear();
  len = radio->read(packetbuf_dataptr(), PACKETBUF_SIZE);
  packetbuf_set_datalen(len);
  return len;
}
/*---------------------------------------------------------------------------*/
static void
set_receive_function(void (* recv)(const struct mac_driver *))
{
  receiver_callback = recv;
}
/*---------------------------------------------------------------------------*/
static int
on(void)
{
  return radio->on();
}
/*---------------------------------------------------------------------------*/
static int
off(int keep_radio_on)
{
  if(keep_radio_on) {
    return radio->on();
  } else {
    return radio->off();
  }
}
/*---------------------------------------------------------------------------*/
static unsigned short
channel_check_interval(void)
{
  return 0;
}
/*---------------------------------------------------------------------------*/
const struct mac_driver nullmac_driver = {
  "emergency_mac",
  emergency_mac_init,
  send_packet,
  read_packet,
  set_receive_function,
  on,
  off,
  channel_check_interval,
};
/*---------------------------------------------------------------------------*/
const struct mac_driver *
emergency_mac_init(const struct radio_driver *d)
{
  radio = d;
  radio->set_receive_function(input_packet);
  radio->on();
  return &emergency_driver;
}
/*---------------------------------------------------------------------------*/
