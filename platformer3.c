/*	example code for cc65, for NES
 *  Scrolling Right with metatile system
 *	, basic platformer
 *	, adding music, sfx
 *	using neslib
 *	Doug Fraker 2018
 */	
 
#include "LIB/neslib.h"
#include "LIB/nesdoug.h"
#include "Sprites.h" // holds our metasprite data
#include "platformer3.h"




	
	
void main (void) {
	
	ppu_off(); // screen off
	
	// load the palettes
	pal_bg(palette_bg);
	pal_spr(palette_sp);

	// use the second set of tiles for sprites
	// both bg and sprites are set to 0 by default
	bank_spr(1);

	set_vram_buffer(); // do at least once
	clear_vram_buffer();

	load_room();
	
	song = 0;
	music_play(song);

	ppu_on_all(); // turn on screen
	
	

	
	while (1){
		// infinite loop
		ppu_wait_nmi(); // wait till beginning of the frame
		
		set_music_speed(8);
		
		pad1 = pad_poll(0); // read the first controller
		pad1_new = get_pad_new(0);
		
		clear_vram_buffer(); // do at the beginning of each frame
		
		movement();
		// set scroll
		set_scroll_x(scroll_x);
		set_scroll_y(scroll_y);
		draw_screen_R();
		draw_sprites();
		
		change_song();
		check_sfx();
	}
}




void load_room(void){
	set_data_pointer(Rooms[0]);
	set_mt_pointer(metatiles1); 
	for(y=0; ;y+=0x20){
		for(x=0; ;x+=0x20){
			clear_vram_buffer(); // do each frame, and before putting anything in the buffer
			address = get_ppu_addr(0, x, y);
			index = (y & 0xf0) + (x >> 4);
			buffer_4_mt(address, index); // ppu_address, index to the data
			flush_vram_update_nmi();
			if (x == 0xe0) break;
		}
		if (y == 0xe0) break;
	}
	
	
	
	// a little bit in the next room
	set_data_pointer(Rooms[1]);
	for(y=0; ;y+=0x20){
		x = 0;
		clear_vram_buffer(); // do each frame, and before putting anything in the buffer
		address = get_ppu_addr(1, x, y);
		index = (y & 0xf0);
		buffer_4_mt(address, index); // ppu_address, index to the data
		flush_vram_update_nmi();
		if (y == 0xe0) break;
	}
	clear_vram_buffer();
	
	// copy the room to the collision map
	// the second one should auto-load with the scrolling code
	memcpy (c_map, Rooms[0], 240);
}




void draw_sprites(void){
	// clear all sprites from sprite buffer
	oam_clear();

	// reset index into the sprite buffer
	sprid = 0;
	
	// draw 1 metasprite
	if(direction == LEFT) {
		sprid = oam_meta_spr(high_byte(BoxGuy1.x), high_byte(BoxGuy1.y), sprid, RoundSprL);
	}
	else{
		sprid = oam_meta_spr(high_byte(BoxGuy1.x), high_byte(BoxGuy1.y), sprid, RoundSprR);
	}
}
	

	
	
void movement(void){
	
// handle x

	old_x = BoxGuy1.x;
	
	if(pad1 & PAD_LEFT){
		direction = LEFT;
		if(BoxGuy1.x <= 0x100) {
			BoxGuy1.vel_x = 0;
			BoxGuy1.x = 0x100;
		}
		else if(BoxGuy1.x < 0x400) { // don't want to wrap around to the other side
			BoxGuy1.vel_x = -0x100;
		}
		else {
			BoxGuy1.vel_x -= ACCEL;
			if(BoxGuy1.vel_x < -MAX_SPEED) BoxGuy1.vel_x = -MAX_SPEED;
		}
	}
	else if (pad1 & PAD_RIGHT){
		
		direction = RIGHT;

		BoxGuy1.vel_x += ACCEL;
		if(BoxGuy1.vel_x > MAX_SPEED) BoxGuy1.vel_x = MAX_SPEED;
	}
	else { // nothing pressed
		if(BoxGuy1.vel_x >= 0x100) BoxGuy1.vel_x -= ACCEL;
		else if(BoxGuy1.vel_x < -0x100) BoxGuy1.vel_x += ACCEL;
		else BoxGuy1.vel_x = 0;
	}
	
	BoxGuy1.x += BoxGuy1.vel_x;
	
	if(BoxGuy1.x > 0xf800) { // make sure no wrap around to the other side
		BoxGuy1.x = 0x100;
		BoxGuy1.vel_x = 0;
	} 
	
	L_R_switch = 1; // shinks the y values in bg_coll, less problems with head / feet collisions
	
	Generic.x = high_byte(BoxGuy1.x); // this is much faster than passing a pointer to BoxGuy1
	Generic.y = high_byte(BoxGuy1.y);
	Generic.width = HERO_WIDTH;
	Generic.height = HERO_HEIGHT;
	bg_collision();
	if(collision_R && collision_L){ // if both true, probably half stuck in a wall
		BoxGuy1.x = old_x;
		BoxGuy1.vel_x = 0;
	}
	else if(collision_L) {
		BoxGuy1.vel_x = 0;
		high_byte(BoxGuy1.x) = high_byte(BoxGuy1.x) - eject_L;
		
	}
	else if(collision_R) {
		BoxGuy1.vel_x = 0;
		high_byte(BoxGuy1.x) = high_byte(BoxGuy1.x) - eject_R;
	} 


	
// handle y

// gravity

	// BoxGuy1.vel_y is signed
	if(BoxGuy1.vel_y < 0x300){
		BoxGuy1.vel_y += GRAVITY;
	}
	else{
		BoxGuy1.vel_y = 0x300; // consistent
	}
	BoxGuy1.y += BoxGuy1.vel_y;
	
	L_R_switch = 0;
	Generic.x = high_byte(BoxGuy1.x); // the rest should be the same
	Generic.y = high_byte(BoxGuy1.y);
	bg_collision();
	
	if(collision_U) {
		high_byte(BoxGuy1.y) = high_byte(BoxGuy1.y) - eject_U;
		BoxGuy1.vel_y = 0;
	}
	else if(collision_D) {
		high_byte(BoxGuy1.y) = high_byte(BoxGuy1.y) - eject_D;
		BoxGuy1.y &= 0xff00;
		if(BoxGuy1.vel_y > 0) {
			BoxGuy1.vel_y = 0;
		}
	}


	// check collision down a little lower than hero
	Generic.y = high_byte(BoxGuy1.y); // the rest should be the same
	bg_check_low();
	if(collision_D) {
		if(pad1_new & PAD_A) {
			BoxGuy1.vel_y = JUMP_VEL; // JUMP
			sfx_play(SFX_JUMP, 0);
		}
		
	}
	
	// do we need to load a new collision map? (scrolled into a new room)
	if((scroll_x & 0xff) < 4){
		new_cmap();
	}
	
// scroll
	temp5 = BoxGuy1.x;
	if (BoxGuy1.x > MAX_RIGHT){
		temp1 = (BoxGuy1.x - MAX_RIGHT) >> 8;
		scroll_x += temp1;
		high_byte(BoxGuy1.x) = high_byte(BoxGuy1.x) - temp1;
	}

	if(scroll_x >= MAX_SCROLL) {
		scroll_x = MAX_SCROLL; // stop scrolling right, end of level
		BoxGuy1.x = temp5; // but allow the x position to go all the way right
		if(high_byte(BoxGuy1.x) >= 0xf1) {
			BoxGuy1.x = 0xf100;
		}
	}
}	




void bg_collision(void){
	// note, uses bits in the metatile data to determine collision
	// sprite collision with backgrounds
	// load the object's x,y,width,height to Generic, then call this
	

	collision_L = 0;
	collision_R = 0;
	collision_U = 0;
	collision_D = 0;
	
	if(Generic.y >= 0xf0) return;
	
	temp6 = temp5 = Generic.x + scroll_x; // upper left (temp6 = save for reuse)
	temp1 = temp5 & 0xff; // low byte x
	temp2 = temp5 >> 8; // high byte x
	
	eject_L = temp1 | 0xf0;
	
	temp3 = Generic.y; // y top
	
	eject_U = temp3 | 0xf0;
	
	if(L_R_switch) temp3 += 2; // fix bug, walking through walls
	
	bg_collision_sub();
	
	if(collision & COL_ALL){ // find a corner in the collision map
		++collision_L;
		++collision_U;
	}
	
	// upper right
	temp5 += Generic.width;
	temp1 = temp5 & 0xff; // low byte x
	temp2 = temp5 >> 8; // high byte x
	
	eject_R = (temp1 + 1) & 0x0f;
	
	// temp3 is unchanged
	bg_collision_sub();
	
	if(collision & COL_ALL){ // find a corner in the collision map
		++collision_R;
		++collision_U;
	}
	
	
	// again, lower
	
	// bottom right, x hasn't changed
	
	temp3 = Generic.y + Generic.height; //y bottom
	if(L_R_switch) temp3 -= 2; // fix bug, walking through walls
	eject_D = (temp3 + 1) & 0x0f;
	if(temp3 >= 0xf0) return;
	
	bg_collision_sub();
	
	if(collision & COL_ALL){ // find a corner in the collision map
		++collision_R;
	}
	if(collision & (COL_DOWN|COL_ALL)){ // find a corner in the collision map
		++collision_D;
	}
	
	// bottom left
	temp1 = temp6 & 0xff; // low byte x
	temp2 = temp6 >> 8; // high byte x
	
	//temp3, y is unchanged

	bg_collision_sub();
	
	if(collision & COL_ALL){ // find a corner in the collision map
		++collision_L;
	}
	if(collision & (COL_DOWN|COL_ALL)){ // find a corner in the collision map
		++collision_D;
	}

	if((temp3 & 0x0f) > 3) collision_D = 0; // for platforms, only collide with the top 3 pixels

}



void bg_collision_sub(void){
	coordinates = (temp1 >> 4) + (temp3 & 0xf0);
	
	map = temp2&1; // high byte
	if(!map){
		collision = c_map[coordinates];
	}
	else{
		collision = c_map2[coordinates];
	}
	
	collision = is_solid[collision];
}



void draw_screen_R(void){
	// scrolling to the right, draw metatiles as we go
	pseudo_scroll_x = scroll_x + 0x120;
	
	temp1 = pseudo_scroll_x >> 8;
	
	set_data_pointer(Rooms[temp1]);
	nt = temp1 & 1;
	x = pseudo_scroll_x & 0xff;
	
	// important that the main loop clears the vram_buffer
	
	switch(scroll_count){
		case 0:
			address = get_ppu_addr(nt, x, 0);
			index = 0 + (x >> 4);
			buffer_4_mt(address, index); // ppu_address, index to the data
			
			address = get_ppu_addr(nt, x, 0x20);
			index = 0x20 + (x >> 4);
			buffer_4_mt(address, index); // ppu_address, index to the data
			break;
			
		case 1:
			address = get_ppu_addr(nt, x, 0x40);
			index = 0x40 + (x >> 4);
			buffer_4_mt(address, index); // ppu_address, index to the data
			
			address = get_ppu_addr(nt, x, 0x60);
			index = 0x60 + (x >> 4);
			buffer_4_mt(address, index); // ppu_address, index to the data
			break;
			
		case 2:
			address = get_ppu_addr(nt, x, 0x80);
			index = 0x80 + (x >> 4);
			buffer_4_mt(address, index); // ppu_address, index to the data
			
			address = get_ppu_addr(nt, x, 0xa0);
			index = 0xa0 + (x >> 4);
			buffer_4_mt(address, index); // ppu_address, index to the data
			break;
			
		default:
			address = get_ppu_addr(nt, x, 0xc0);
			index = 0xc0 + (x >> 4);
			buffer_4_mt(address, index); // ppu_address, index to the data
			
			address = get_ppu_addr(nt, x, 0xe0);
			index = 0xe0 + (x >> 4);
			buffer_4_mt(address, index); // ppu_address, index to the data
	}


	++scroll_count;
	scroll_count &= 3; //mask off top bits, keep it 0-3
}



void new_cmap(void){
	// copy a new collision map to one of the 2 c_map arrays
	room = ((scroll_x >> 8) +1); //high byte = room, one to the right
	
	map = room & 1; //even or odd?
	if(!map){
		memcpy (c_map, Rooms[room], 240);
	}
	else{
		memcpy (c_map2, Rooms[room], 240);
	}
}




void bg_check_low(void){

	// floor collisions
	collision_D = 0;
	
	temp5 = Generic.x + scroll_x;    //left
	temp1 = temp5 & 0xff; //low byte
	temp2 = temp5 >> 8; //high byte
	
	temp3 = Generic.y + Generic.height + 1; // bottom
	
	if(temp3 >= 0xf0) return;
	
	bg_collision_sub();
	
	if(collision & (COL_DOWN|COL_ALL)){ // find a corner in the collision map
		++collision_D;
	}
	
	
	//temp5 = right
	temp5 += Generic.width;
	temp1 = temp5 & 0xff; //low byte
	temp2 = temp5 >> 8; //high byte
	
	//temp3 is unchanged
	bg_collision_sub();
	
	if(collision & (COL_DOWN|COL_ALL)){ // find a corner in the collision map
		++collision_D;
	}
	
	if((temp3 & 0x0f) > 3) collision_D = 0; // for platforms, only collide with the top 3 pixels

}



void change_song(void){
	if(pad1_new & PAD_START) {
		++song;
		if(song >= MAX_SONGS) song = 0;
		music_play(song);
	}
}



void check_sfx(void){
	if(pad1_new & PAD_B) {
		sfx_play(SFX_NOISE, 0);
	}
	if(pad1_new & PAD_SELECT) {
		sfx_play(SFX_DING, 0);
	}
}

