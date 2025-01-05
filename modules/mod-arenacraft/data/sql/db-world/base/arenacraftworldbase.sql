use acore_world;

update playercreateinfo set map=571, zone=394, position_x=4035.6206, position_y=-3756.447, position_z=116.24431, orientation=4.15822; 
delete from playercreateinfo_item; 

delete from spell_script_names where spell_id=90000;
insert into spell_script_names (spell_id, `ScriptName`) values (90000, 'class_rog_marked_for_death');

delete from spell_script_names where spell_id=90001;
insert into spell_script_names (spell_id, `ScriptName`) values (90001, 'class_rog_marked_for_death_aura');

INSERT IGNORE INTO creature_template_spell (`CreatureID`, `Index`, `Spell`, `VerifiedBuild`) values (6112, 1, 80869, null);

delete from creature where map != 571 and `zoneId` != 394 and `areaId` != 395;
