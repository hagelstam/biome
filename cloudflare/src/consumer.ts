import { SensorReading } from './types'

const SENSOR_ID = 'mkr1000'

async function writeToInflux(
  readings: Array<{ id: string; body: SensorReading }>,
  env: Env
): Promise<void> {
  const lines = readings.map(({ body: r }) => {
    const tags = `sensor_id=${SENSOR_ID}`

    const fields = [
      `temperature=${r.temperature}`,
      `humidity=${r.humidity}`,
      `heat_index=${r.heat_index}`,
      `rssi_dbm=${r.rssi_dbm}i`,
      `uptime_s=${r.uptime_s}i`
    ].join(',')

    return `sensor_readings,${tags} ${fields} ${r.timestamp}`
  })

  const body = lines.join('\n')
  const url = `${env.INFLUXDB_URL}/api/v2/write?bucket=${env.INFLUXDB_BUCKET}&precision=s`

  const res = await fetch(url, {
    method: 'POST',
    headers: {
      Authorization: `Token ${env.INFLUXDB_TOKEN}`,
      'Content-Type': 'text/plain; charset=utf-8'
    },
    body
  })

  if (!res.ok) {
    const error = await res.text()
    throw new Error(`InfluxDB write failed: ${res.status} ${error}`)
  }
}

async function consumer(
  batch: MessageBatch<SensorReading>,
  env: Env
): Promise<void> {
  await writeToInflux(
    batch.messages.map((m) => ({ id: m.id, body: m.body })),
    env
  )
  for (const message of batch.messages) {
    message.ack()
  }
}

export { consumer }
