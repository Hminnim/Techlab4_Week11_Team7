-- UAL1_Standard 스켈레탈 메시 액터의 ScriptPath에 연결한 뒤 PIE에서 사용한다.
-- CharacterCamera가 있는 액터는 WASD 이동, Shift 달리기, 좌클릭 공격을 사용한다.
local pack = "Assets/Models/UAL1/UAL1_Standard.fbx"
local locomotionPack = "Assets/Models/UAL1/UAL1_Standard_RM.fbx"
local walkSpeed = 1.0
local sprintSpeed = 3.0
local previewSpeed
local useMovementVelocity
local hasFollowCamera
local ready = false
local attackEnabled = true -- false로 바꾸면 Lua 전이 규칙이 공격을 막는다.
local airTime = 1.8
local jumpHeight = 1.0
local jumpElapsed

local function ResetJump()
    if script_state.jumpGroundZ ~= nil then
        local position = obj.Location
        position.z = script_state.jumpGroundZ
        obj.Location = position
        script_state.jumpGroundZ = nil
    end
    jumpElapsed = nil
end

local function Setup()
    SetAnimRootMotion(locomotionPack, "Walk_Loop", true)
    SetAnimRootMotion(locomotionPack, "Sprint_Loop", true)
    ready = obj:ConfigureCharacterAnimations(
        locomotionPack, pack, pack,
        "Armature|Idle_Loop", "Armature|Sword_Attack", "Armature|Hit_Chest",
        {
            path = pack,
            start = "Armature|Jump_Start",
            loop = "Armature|Jump_Loop",
            land = "Armature|Jump_Land"
        },
        {
            { take = "Idle_Loop", x = 0 },
            { take = "Walk_Loop", x = walkSpeed },
            { take = "Sprint_Loop", x = sprintSpeed }
        }
    )
    previewSpeed = 0
    useMovementVelocity = obj:HasComponent("UMovementComponent")
    hasFollowCamera = GetComponentByName("CharacterCamera") ~= nil
    ResetJump()
    obj:SetAnimAttackRule(function(requested)
        return requested and attackEnabled
    end)
    obj:SetRestartHitOnReentry(script_state.restartHit == true)
    if hasFollowCamera then
        obj:ResetCameraLag()
        SetViewTargetWithBlend(obj, 0)
    end
end

local function UpdateJump(dt)
    if jumpElapsed == nil then
        if not IsKeyPressed("space") or not obj:RequestAnimJump() then return end
        script_state.jumpGroundZ = obj.Location.z
        jumpElapsed = 0
    end

    jumpElapsed = math.min(jumpElapsed + dt, airTime)
    local t = jumpElapsed / airTime
    local position = obj.Location
    position.z = script_state.jumpGroundZ + 4 * jumpHeight * t * (1 - t)
    obj.Location = position

    if t >= 1 then
        script_state.jumpGroundZ = nil
        jumpElapsed = nil
        obj:NotifyAnimLanded()
    end
end

function BeginPlay()
    Setup()
end

function OnReload()
    Setup()
end

local function UpdateLocomotion(dt)
    if useMovementVelocity then
        local velocity = obj.Velocity
        obj:SetAnimSpeed(math.sqrt(velocity.x * velocity.x + velocity.y * velocity.y))
    else
        local moving = IsKeyDown("W")
        if hasFollowCamera then
            local x = (IsKeyDown("S") and 1 or 0) - (moving and 1 or 0)
            local y = (IsKeyDown("A") and 1 or 0) - (IsKeyDown("D") and 1 or 0)
            moving = x ~= 0 or y ~= 0
            if moving then
                -- UAL1 루트 모션의 전진 방향은 로컬 -X다.
                local rotation = obj.Rotation
                rotation:LookDirection(FVector(-x, -y, 0))
                obj.Rotation = rotation
            end
        end
        local target = moving and (IsKeyDown("shift") and sprintSpeed or walkSpeed) or 0
        local step = sprintSpeed * dt
        previewSpeed = previewSpeed + math.max(-step, math.min(step, target - previewSpeed))
        obj:SetAnimSpeed(previewSpeed)
    end
end

function Tick(dt)
    if not ready then return end

    UpdateLocomotion(dt)
    local attackPressed = IsKeyPressed("A")
    if hasFollowCamera then
        attackPressed = IsLeftMouseButtonPressed()
    end
    if attackPressed then
        obj:RequestAnimAttack()
    end
    if IsKeyPressed("H") then
        obj:RequestAnimHit()
    end
    if IsKeyPressed("R") then
        script_state.restartHit = not script_state.restartHit
        obj:SetRestartHitOnReentry(script_state.restartHit)
    end
    UpdateJump(dt)
end

function EndPlay()
    ResetJump()
end

function OnAnimNotify(notifyName, clipName)
    print("[Lua AnimNotify] '" .. tostring(notifyName) .. "' from clip '" .. tostring(clipName) .. "'")
end

function OnAnimNotifyBegin(notifyName, clipName, duration)
    print(string.format("[Lua AnimNotifyState] Begin '%s' from clip '%s' (Duration: %.3fs)",
        tostring(notifyName), tostring(clipName), tonumber(duration) or 0))
end

-- function OnAnimNotifyTick(notifyName, clipName, dt)
-- end

function OnAnimNotifyEnd(notifyName, clipName)
    print(string.format("[Lua AnimNotifyState] End '%s' from clip '%s'",
        tostring(notifyName), tostring(clipName)))
end
