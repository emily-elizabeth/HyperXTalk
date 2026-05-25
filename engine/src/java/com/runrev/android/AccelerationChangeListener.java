package com.runrev.android;

import android.hardware.*;
import android.content.*;

// acceleration detection class

abstract class AccelerationChangeListener implements SensorEventListener
{
	class AccelerationNotSupportedException extends Exception
	{
	}
	
	private SensorManager m_sensorMgr;
	private Sensor m_accelerometer;
	private boolean m_listening;
	private boolean m_paused;
	private boolean m_registered;
	
	private int m_sensor_delay = SensorManager.SENSOR_DELAY_GAME;
	
	public AccelerationChangeListener(Context p_context, int p_delay)
	{
		m_sensor_delay = p_delay;
		m_sensorMgr = (SensorManager)p_context.getSystemService(Context.SENSOR_SERVICE);
		m_listening = false;
		m_paused = false;
		m_registered = false;
		m_accelerometer = m_sensorMgr.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
	}
	
	private void register()
	{
		if (!m_registered && m_listening && !m_paused)
		{
			m_registered = m_sensorMgr.registerListener(this, m_accelerometer, m_sensor_delay);
		}
	}
	
	private void unregister()
	{
		if (m_registered && (!m_listening || m_paused))
		{
			m_sensorMgr.unregisterListener(this);
			m_registered = false;
		}
	}
	
	public void setListening(boolean p_listening)
	{
		m_listening = p_listening;
		if (m_listening)
			register();
		else
			unregister();
	}	
	
	public void onPause()
	{
		m_paused = true;
		unregister();
	}
	
	public void onResume()
	{
		m_paused = false;
		register();
	}
	
	public void onAccuracyChanged(Sensor sensor, int accuracy)
	{
	}

	public void onSensorChanged(SensorEvent event)
	{
		if (event.sensor.getType() == Sensor.TYPE_ACCELEROMETER)
		{
			float t_x, t_y, t_z;
			t_x = event.values[0];
			t_y = event.values[1];
			t_z = event.values[2];
			onAccelerationChanged(t_x, t_y, t_z, event.timestamp);
		}
	}
	
	public abstract void onAccelerationChanged(float x, float y, float z, long timestamp);
}
