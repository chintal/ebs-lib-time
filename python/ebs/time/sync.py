
import time
from .systime import ModbusTimeMixin


class ModbusTimeSyncMixin(ModbusTimeMixin):
    _time_sync_request_bit = 1 << 1

    def __init__(self, *args, **kwargs):
        self._descriptors = {}
        super(ModbusTimeSyncMixin, self).__init__(*args, **kwargs)

    def _build_host_timestamp(self):
        return self.pack_timestamp(time.time())

    def sync_time(self, force=False):
        if not force and \
                not self.exception_status & self._time_sync_request_bit:
            return

        _write_address = self._time_base_address + 3
        _trigger_address = self._time_base_address + 5

        self.write_registers(address=_write_address,
                             values=self._build_host_timestamp())

        self.readwrite_registers(
            address=self._time_base_address, read_count=3,
            write_address=_trigger_address, write_registers=[0x0000]
        )

        self.write_registers(address=_write_address,
                             values=self._build_host_timestamp())

    def __getattr__(self, item):
        return super(ModbusTimeSyncMixin, self).__getattr__(item)
