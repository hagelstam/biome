import { zValidator } from '@hono/zod-validator'
import { Hono } from 'hono'
import { SensorReadingSchema } from './types'

const producer = new Hono<{ Bindings: Env }>()

producer.use('/ingest', async (c, next) => {
  if (c.req.header('X-Producer-Secret') !== c.env.PRODUCER_SECRET) {
    return c.json({ error: 'unauthorized' }, 401)
  }
  await next()
})

producer.post(
  '/ingest',
  zValidator('json', SensorReadingSchema, (result, c) => {
    if (!result.success) {
      console.log({
        msg: 'invalid payload',
        payload: c.req.json()
      })
      return c.json(
        { error: 'invalid payload', issues: result.error.issues },
        400
      )
    }
  }),
  async (c) => {
    const reading = c.req.valid('json')
    try {
      await c.env.SENSOR_READING_QUEUE.send(reading)
    } catch (err) {
      console.error({ msg: 'enqueue failed', err: String(err) })
      return c.json({ error: 'failed to enqueue' }, 500)
    }
    console.log({ msg: 'reading queued', reading })
    return c.json({ status: 'queued' }, 202)
  }
)

export { producer }
