import unittest

from protocol import FLASH_BYTES, READ_COMMAND, WRITE_COMMAND, expected_schedule
from stage import STAGE_HEADER, pack_stage


class GeometryTests(unittest.TestCase):
	def test_agent_commands_are_distinct_single_bytes(self):
		self.assertEqual(READ_COMMAND, b"D")
		self.assertEqual(WRITE_COMMAND, b"W")

	def test_schedule_covers_every_sector_once_and_boot_sector_last(self):
		schedule = expected_schedule()
		addresses = [address for _, address, _ in schedule]
		self.assertEqual(len(addresses), FLASH_BYTES // 0x1000)
		self.assertEqual(len(set(addresses)), len(addresses))
		self.assertEqual(addresses[-1], 0)
		self.assertEqual(sorted(addresses), list(range(0, FLASH_BYTES, 0x1000)))

	def test_only_first_block_uses_sector_erase(self):
		for _, address, mode in expected_schedule():
			self.assertEqual(mode, "S" if address < 0x10000 else "B")

	def test_stage_envelope_is_bounded(self):
		envelope = pack_stage(bytes(range(256)) * 4)
		self.assertEqual(envelope[:8], b"SV6STG01")
		self.assertEqual(len(envelope[:STAGE_HEADER.size]), 80)


if __name__ == "__main__":
	unittest.main()
