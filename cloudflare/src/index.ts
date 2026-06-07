import { consumer } from './consumer'
import { producer } from './producer'

export default { fetch: producer.fetch, queue: consumer }
