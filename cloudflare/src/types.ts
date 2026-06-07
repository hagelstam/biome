import z from 'zod'

export const SensorReadingSchema = z.object({
  timestamp: z.int().nonnegative(),
  temperature: z.number(),
  humidity: z.number().min(0).max(100),
  heat_index: z.number(),
  uptime_s: z.number().int().nonnegative(),
  rssi_dbm: z.number().int().max(0)
})

export type SensorReading = z.infer<typeof SensorReadingSchema>
