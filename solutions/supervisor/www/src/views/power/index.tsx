import { Button ,Toast} from 'antd-mobile'
import { setDevicePowerApi } from '@/api/device/index'
import { PowerMode } from '@/enum'

function Power() {
	const onOperateDevice = async (mode: PowerMode) => {
		await setDevicePowerApi({ mode })
		Toast.show({
			icon: 'success',
			content: 'Operate Success',
		})
	}

	return (
		<div className='flex h-full justify-center p-32 flex-col text-18'>
			<Button className='mb-12' block onClick={() => onOperateDevice(PowerMode.Restart)}>
				<span className='text-error text-18'>Reboot</span>
			</Button>
			<Button className='mb-12' block onClick={() => onOperateDevice(PowerMode.Shutdown)}>
				<span className='text-error text-18'>Shutdown</span>
			</Button>
		</div>
	)
}

export default Power
