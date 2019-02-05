import { observable } from 'mobx'
import { computed } from 'mobx'

import { memoize } from '../../base/memoize'
import { Vector3 } from '../../math/vector3'
import { AppModel } from '../app/model'

import { LocalisationRobotModel } from './darwin_robot/model'
import { FieldModel } from './field/model'
import { SkyboxModel } from './skybox/model'

export class TimeModel {
  @observable time: number // seconds
  @observable lastRenderTime: number // seconds

  constructor({ time, lastRenderTime }: { time: number, lastRenderTime: number }) {
    this.time = time
    this.lastRenderTime = lastRenderTime
  }

  static of() {
    return new TimeModel({
      time: 0,
      lastRenderTime: 0,
    })
  }

  @computed get timeSinceLastRender() {
    return this.time - this.lastRenderTime
  }
}

export enum ViewMode {
  FreeCamera,
  FirstPerson,
  ThirdPerson,
}

class CameraModel {
  @observable position: Vector3
  @observable yaw: number
  @observable pitch: number
  @observable distance: number

  constructor({ position, yaw, pitch, distance }: CameraModel) {
    this.position = position
    this.yaw = yaw
    this.pitch = pitch
    this.distance = distance
  }

  static of() {
    return new CameraModel({
      position: new Vector3(-1, 0, 1),
      yaw: 0,
      pitch: -Math.PI / 4,
      distance: 0.5,
    })
  }
}

export class ControlsModel {
  @observable forward: boolean
  @observable left: boolean
  @observable right: boolean
  @observable back: boolean
  @observable up: boolean
  @observable down: boolean
  @observable pitch: number
  @observable yaw: number

  constructor({ forward, left, right, back, up, down, pitch, yaw }: ControlsModel) {
    this.forward = forward
    this.left = left
    this.right = right
    this.back = back
    this.up = up
    this.down = down
    this.pitch = pitch
    this.yaw = yaw
  }

  static of() {
    return new ControlsModel({
      forward: false,
      left: false,
      right: false,
      back: false,
      up: false,
      down: false,
      pitch: 0,
      yaw: 0,
    })
  }
}

export class LocalisationModel {
  @observable private appModel: AppModel
  @observable field: FieldModel
  @observable skybox: SkyboxModel
  @observable camera: CameraModel
  @observable locked: boolean
  @observable controls: ControlsModel
  @observable viewMode: ViewMode
  @observable target?: LocalisationRobotModel
  @observable time: TimeModel

  constructor(appModel: AppModel, { field, skybox, camera, locked, controls, viewMode, target, time }: {
    field: FieldModel,
    skybox: SkyboxModel,
    camera: CameraModel,
    locked: boolean,
    controls: ControlsModel,
    viewMode: ViewMode,
    target?: LocalisationRobotModel,
    time: TimeModel
  }) {
    this.appModel = appModel
    this.field = field
    this.skybox = skybox
    this.camera = camera
    this.locked = locked
    this.controls = controls
    this.viewMode = viewMode
    this.target = target
    this.time = time
  }

  static of = memoize((appModel: AppModel): LocalisationModel => {
    return new LocalisationModel(appModel, {
      field: FieldModel.of(),
      skybox: SkyboxModel.of(),
      camera: CameraModel.of(),
      locked: false,
      controls: ControlsModel.of(),
      viewMode: ViewMode.FreeCamera,
      time: TimeModel.of(),
    })
  })

  @computed get robots(): LocalisationRobotModel[] {
    return this.appModel.robots.map(robot => LocalisationRobotModel.of(robot))
  }
}
