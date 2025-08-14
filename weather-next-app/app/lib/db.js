import { Pool } from 'pg';

const pool = new Pool({
  host: '192.168.108.11',
  port: 5432,
  user: 'markus',
  password: 'Datait2025!', 
  database: 'Weather', 
  ssl: false
});

export async function getLastWeatherData() {
  const { rows } = await pool.query(
    `SELECT id, device_id, timestamp, temperature, humidity, pressure, tvoc, eco2
     FROM weatherdata
     ORDER BY timestamp DESC
     LIMIT 1`
  );
  return JSON.stringify(rows[0] || {});
}